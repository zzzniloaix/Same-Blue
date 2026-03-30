#include "hdr_probe.h"

#include <cmath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/mastering_display_metadata.h>
}

namespace hdr {

HDRProbe::HDRProbe(const std::string& filepath) {
    load_video_file(filepath);
}

void HDRProbe::load_video_file(const std::string& filepath) {
    clear_cache();
    
    // Open the video file using our RAII wrapper
    format_context_.open(filepath);
    
    // Find the first video stream
    find_video_stream();
    
    // Parse color space and HDR information
    parse_color_space_info();
    parse_hdr_metadata();
}

std::optional<ColorPrimaries> HDRProbe::get_color_primaries() const {
    return color_primaries_;
}

std::optional<TransferFunction> HDRProbe::get_transfer_characteristics() const {
    return transfer_function_;
}

std::optional<ColorSpace> HDRProbe::get_color_space() const {
    return color_space_;
}

std::optional<MasteringDisplay> HDRProbe::get_mastering_display_info() const {
    return mastering_display_;
}

std::optional<ContentLightLevel> HDRProbe::get_content_light_level() const {
    return content_light_level_;
}

std::optional<uint16_t> HDRProbe::get_max_content_light_level() const {
    if (content_light_level_) {
        return content_light_level_->max_content_light_level;
    }
    return std::nullopt;
}

bool HDRProbe::has_hdr_metadata() const {
    return mastering_display_.has_value() || content_light_level_.has_value();
}

bool HDRProbe::is_hdr_content() const {
    if (!transfer_function_) {
        return false;
    }
    
    return transfer_function_ == TransferFunction::PQ || 
           transfer_function_ == TransferFunction::HLG;
}

int HDRProbe::get_width() const {
    if (!is_loaded() || video_stream_index_ < 0) {
        return 0;
    }
    
    AVStream* video_stream = format_context_->streams[video_stream_index_];
    return video_stream->codecpar->width;
}

int HDRProbe::get_height() const {
    if (!is_loaded() || video_stream_index_ < 0) {
        return 0;
    }
    
    AVStream* video_stream = format_context_->streams[video_stream_index_];
    return video_stream->codecpar->height;
}

int HDRProbe::get_pixel_format() const {
    if (!is_loaded() || video_stream_index_ < 0) {
        return AV_PIX_FMT_NONE;
    }
    
    AVStream* video_stream = format_context_->streams[video_stream_index_];
    return video_stream->codecpar->format;
}

bool HDRProbe::is_loaded() const {
    return format_context_.is_open() && video_stream_index_ >= 0;
}

void HDRProbe::find_video_stream() {
    video_stream_index_ = -1;
    
    for (unsigned int i = 0; i < format_context_->nb_streams; i++) {
        if (format_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = static_cast<int>(i);
            break;
        }
    }
    
    if (video_stream_index_ < 0) {
        throw std::runtime_error("No video stream found in file: " + format_context_.filename());
    }
}

void HDRProbe::parse_color_space_info() {
    if (!is_loaded()) {
        return;
    }
    
    AVStream* video_stream = format_context_->streams[video_stream_index_];
    AVCodecParameters* codecpar = video_stream->codecpar;
    
    // Parse color primaries — guard against values outside our enum range
    if (codecpar->color_primaries != AVCOL_PRI_UNSPECIFIED &&
        codecpar->color_primaries >= 0 &&
        codecpar->color_primaries <= static_cast<int>(ColorPrimaries::EBU3213))
        color_primaries_ = static_cast<ColorPrimaries>(codecpar->color_primaries);

    // Parse transfer characteristics
    if (codecpar->color_trc != AVCOL_TRC_UNSPECIFIED &&
        codecpar->color_trc >= 0 &&
        codecpar->color_trc <= static_cast<int>(TransferFunction::HLG))
        transfer_function_ = static_cast<TransferFunction>(codecpar->color_trc);

    // Parse color space (matrix coefficients)
    if (codecpar->color_space != AVCOL_SPC_UNSPECIFIED &&
        codecpar->color_space >= 0 &&
        codecpar->color_space <= static_cast<int>(ColorSpace::ICTCP))
        color_space_ = static_cast<ColorSpace>(codecpar->color_space);
}

void HDRProbe::parse_hdr_metadata() {
    if (!is_loaded()) {
        return;
    }

    AVStream* video_stream = format_context_->streams[video_stream_index_];

    // --- Mastering display (HDR10 MDCV / SMPTE 2086) ---
    const AVPacketSideData* mdcv_sd = av_packet_side_data_get(
        video_stream->codecpar->coded_side_data,
        video_stream->codecpar->nb_coded_side_data,
        AV_PKT_DATA_MASTERING_DISPLAY_METADATA
    );

    if (mdcv_sd) {
        const auto* mdcv = reinterpret_cast<const AVMasteringDisplayMetadata*>(mdcv_sd->data);
        MasteringDisplay md{};

        // FFmpeg primaries order: [0]=R [1]=G [2]=B, dimension [x=0, y=1]
        // Our struct order:        [0]=G [1]=B [2]=R
        if (mdcv->has_primaries) {
            md.display_primaries_x[0] = static_cast<uint16_t>(std::round(av_q2d(mdcv->display_primaries[1][0]) * 50000.0)); // G
            md.display_primaries_x[1] = static_cast<uint16_t>(std::round(av_q2d(mdcv->display_primaries[2][0]) * 50000.0)); // B
            md.display_primaries_x[2] = static_cast<uint16_t>(std::round(av_q2d(mdcv->display_primaries[0][0]) * 50000.0)); // R

            md.display_primaries_y[0] = static_cast<uint16_t>(std::round(av_q2d(mdcv->display_primaries[1][1]) * 50000.0)); // G
            md.display_primaries_y[1] = static_cast<uint16_t>(std::round(av_q2d(mdcv->display_primaries[2][1]) * 50000.0)); // B
            md.display_primaries_y[2] = static_cast<uint16_t>(std::round(av_q2d(mdcv->display_primaries[0][1]) * 50000.0)); // R

            md.white_point_x = static_cast<uint16_t>(std::round(av_q2d(mdcv->white_point[0]) * 50000.0));
            md.white_point_y = static_cast<uint16_t>(std::round(av_q2d(mdcv->white_point[1]) * 50000.0));
        }

        if (mdcv->has_luminance) {
            md.max_display_mastering_luminance = static_cast<uint32_t>(std::round(av_q2d(mdcv->max_luminance) * 10000.0));
            md.min_display_mastering_luminance = static_cast<uint32_t>(std::round(av_q2d(mdcv->min_luminance) * 10000.0));
        }

        // FIX 12: only store when both fields are valid — partial data is useless for HDR signaling
        if (mdcv->has_primaries && mdcv->has_luminance)
            mastering_display_ = md;
    }

    // --- Content light level (HDR10 CLLI / CTA-861.3) ---
    const AVPacketSideData* clli_sd = av_packet_side_data_get(
        video_stream->codecpar->coded_side_data,
        video_stream->codecpar->nb_coded_side_data,
        AV_PKT_DATA_CONTENT_LIGHT_LEVEL
    );

    if (clli_sd) {
        const auto* clli = reinterpret_cast<const AVContentLightMetadata*>(clli_sd->data);
        content_light_level_ = ContentLightLevel{
            static_cast<uint16_t>(clli->MaxCLL),
            static_cast<uint16_t>(clli->MaxFALL)
        };
    }
}

void HDRProbe::clear_cache() {
    video_stream_index_ = -1;
    color_primaries_.reset();
    transfer_function_.reset();
    color_space_.reset();
    mastering_display_.reset();
    content_light_level_.reset();
}

} // namespace hdr