#include "pipeline.h"

#include "../process/color_convert.h"

#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <vector>
#include <iostream>
#include <cstdio>   // std::remove

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

namespace encode {

// ---------------------------------------------------------------------------
// RAII wrappers
// ---------------------------------------------------------------------------
namespace {

struct InputFmtDeleter {
    void operator()(AVFormatContext* c) noexcept { avformat_close_input(&c); }
};
using InputFmtPtr = std::unique_ptr<AVFormatContext, InputFmtDeleter>;

struct OutputFmtDeleter {
    void operator()(AVFormatContext* c) noexcept {
        if (!c) return;
        if (c->pb && !(c->oformat->flags & AVFMT_NOFILE))
            avio_closep(&c->pb);
        avformat_free_context(c);
    }
};
using OutputFmtPtr = std::unique_ptr<AVFormatContext, OutputFmtDeleter>;

struct CodecCtxDeleter {
    void operator()(AVCodecContext* c) noexcept { avcodec_free_context(&c); }
};
using CodecCtxPtr = std::unique_ptr<AVCodecContext, CodecCtxDeleter>;

struct FrameDeleter {
    void operator()(AVFrame* f) noexcept { av_frame_free(&f); }
};
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;

struct PacketDeleter {
    void operator()(AVPacket* p) noexcept { av_packet_free(&p); }
};
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

} // namespace

// ---------------------------------------------------------------------------
// Per-frame pixel processing
//
// Converts a decoded YUV frame to SDR yuv420p.
// Handles 10-bit (yuv420p10le) and 8-bit (yuv420p) source formats.
// For 8-bit SDR input: pixels are copied directly (no color modification).
// For 10-bit HDR input: tone mapping + BT.2020→BT.709 applied per 2×2 block.
// ---------------------------------------------------------------------------
static void process_frame_sdr(const AVFrame* src, AVFrame* dst, double peak_nits) {
    if (dst->format != AV_PIX_FMT_YUV420P)
        throw std::runtime_error("Expected yuv420p output frame");

    const bool is_10bit = (src->format == AV_PIX_FMT_YUV420P10LE);
    const bool is_8bit  = (src->format == AV_PIX_FMT_YUV420P);

    if (!is_10bit && !is_8bit)
        throw std::runtime_error("Unsupported source pixel format for SDR conversion");

    // FIX 10: reject full-range input — our YCbCr offsets assume limited range
    if (src->color_range == AVCOL_RANGE_JPEG)
        throw std::runtime_error("Full-range (JPEG) YCbCr input is not supported; "
                                  "re-encode source with limited-range signaling");

    const int w = src->width;
    const int h = src->height;

    // For 8-bit SDR → SDR: straight copy, no colour math needed
    if (is_8bit) {
        for (int y = 0; y < h; ++y)
            std::copy_n(src->data[0] + y * src->linesize[0], w,
                        dst->data[0] + y * dst->linesize[0]);
        for (int y = 0; y < h / 2; ++y) {
            std::copy_n(src->data[1] + y * src->linesize[1], w / 2,
                        dst->data[1] + y * dst->linesize[1]);
            std::copy_n(src->data[2] + y * src->linesize[2], w / 2,
                        dst->data[2] + y * dst->linesize[2]);
        }
        return;
    }

    // 10-bit HDR → 8-bit SDR
    // linesize is in bytes; divide by 2 to get the uint16_t stride
    const int y_stride_src  = src->linesize[0] / 2;
    const int cb_stride_src = src->linesize[1] / 2;
    const int cr_stride_src = src->linesize[2] / 2;  // FIX 5: use linesize[2] for Cr, not linesize[1]

    const auto* Y_src  = reinterpret_cast<const uint16_t*>(src->data[0]);
    const auto* Cb_src = reinterpret_cast<const uint16_t*>(src->data[1]);
    const auto* Cr_src = reinterpret_cast<const uint16_t*>(src->data[2]);

    // Iterate over 2×2 chroma blocks (one U/V sample per block in 4:2:0)
    for (int by = 0; by < h / 2; ++by) {
        for (int bx = 0; bx < w / 2; ++bx) {
            const int u10 = Cb_src[by * cb_stride_src + bx];
            const int v10 = Cr_src[by * cr_stride_src + bx];

            // Accumulate gamma-encoded BT.709 RGB for the 4 luma samples
            double sum_r = 0.0, sum_g = 0.0, sum_b = 0.0;

            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int px = bx * 2 + dx;
                    const int py = by * 2 + dy;
                    const int y10 = Y_src[py * y_stride_src + px];

                    // HDR10 YCbCr → PQ decode → tone map → BT.2020→BT.709 matrix
                    process::Vec3 sdr_linear = process::HDR10_to_SDR(y10, u10, v10, peak_nits);

                    // BT.709 OETF (gamma encode)
                    process::Vec3 sdr_gamma = process::LinearToBT709(sdr_linear);

                    // Y output (BT.709 luma)
                    const double Y709 = 0.2126 * sdr_gamma.r
                                      + 0.7152 * sdr_gamma.g
                                      + 0.0722 * sdr_gamma.b;
                    dst->data[0][py * dst->linesize[0] + px] = static_cast<uint8_t>(
                        std::clamp(std::round(Y709 * 219.0 + 16.0), 16.0, 235.0));

                    sum_r += sdr_gamma.r;
                    sum_g += sdr_gamma.g;
                    sum_b += sdr_gamma.b;
                }
            }

            // Chroma: average the 4 BT.709 RGB values then convert
            const process::Vec3 avg = { sum_r * 0.25, sum_g * 0.25, sum_b * 0.25 };
            const process::YCbCr8 chroma = process::RGB_BT709_to_YCbCr8(avg);

            dst->data[1][by * dst->linesize[1] + bx] = chroma.cb;
            dst->data[2][by * dst->linesize[2] + bx] = chroma.cr;
        }
    }
}

// ---------------------------------------------------------------------------
// transcode
// ---------------------------------------------------------------------------
void transcode(const std::string& input,
               const std::string& output,
               const Config& cfg)
{
    // FIX 11: open input once and detect HDR from stream metadata — no separate HDRProbe call
    AVFormatContext* ifmt_raw = nullptr;
    if (avformat_open_input(&ifmt_raw, input.c_str(), nullptr, nullptr) < 0)
        throw std::runtime_error("Cannot open input: " + input);
    InputFmtPtr ifmt(ifmt_raw);

    if (avformat_find_stream_info(ifmt.get(), nullptr) < 0)
        throw std::runtime_error("Cannot read stream info from: " + input);

    // Find best video stream
    const int video_idx = av_find_best_stream(
        ifmt.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_idx < 0)
        throw std::runtime_error("No video stream in: " + input);

    AVStream* in_video = ifmt->streams[video_idx];

    // FIX 9: reject odd dimensions — our 4:2:0 loop assumes even width and height
    if (in_video->codecpar->width  % 2 != 0 ||
        in_video->codecpar->height % 2 != 0)
        throw std::runtime_error("Odd video dimensions not supported: "
            + std::to_string(in_video->codecpar->width) + "x"
            + std::to_string(in_video->codecpar->height));

    // FIX 11 (cont.): detect HDR from codec params of the already-open stream
    const bool input_is_hdr = (in_video->codecpar->color_trc == AVCOL_TRC_SMPTE2084 ||
                                in_video->codecpar->color_trc == AVCOL_TRC_ARIB_STD_B67);
    const bool output_hdr =
        (cfg.mode == OutputMode::HDR) ||
        (cfg.mode == OutputMode::Auto && input_is_hdr);

    std::cout << "Input:  " << input  << "\n";
    std::cout << "Output: " << output << "\n";
    std::cout << "Mode:   " << (output_hdr ? "HDR (BT.2020 PQ 10-bit)" : "SDR (BT.709 8-bit)") << "\n";

    // --- Open decoder ---
    const AVCodec* decoder = avcodec_find_decoder(in_video->codecpar->codec_id);
    if (!decoder)
        throw std::runtime_error("No decoder found for codec id");

    CodecCtxPtr dec_ctx(avcodec_alloc_context3(decoder));
    if (!dec_ctx)
        throw std::runtime_error("Failed to alloc decoder context");

    if (avcodec_parameters_to_context(dec_ctx.get(), in_video->codecpar) < 0)
        throw std::runtime_error("Failed to copy codec parameters to decoder");

    dec_ctx->thread_count = 0; // auto-detect thread count

    if (avcodec_open2(dec_ctx.get(), decoder, nullptr) < 0)
        throw std::runtime_error("Failed to open decoder");

    // --- Open output container first (needed to check AVFMT_GLOBALHEADER) ---
    AVFormatContext* ofmt_raw = nullptr;
    if (avformat_alloc_output_context2(
            &ofmt_raw, nullptr, nullptr, output.c_str()) < 0)
        throw std::runtime_error("Failed to create output context for: " + output);
    OutputFmtPtr ofmt(ofmt_raw);

    // --- Open encoder ---
    const AVCodec* encoder = avcodec_find_encoder_by_name("libsvtav1");
    if (!encoder)
        throw std::runtime_error("libsvtav1 not found — rebuild FFmpeg with SVT-AV1");

    CodecCtxPtr enc_ctx(avcodec_alloc_context3(encoder));
    if (!enc_ctx)
        throw std::runtime_error("Failed to alloc encoder context");

    enc_ctx->width   = dec_ctx->width;
    enc_ctx->height  = dec_ctx->height;
    enc_ctx->pix_fmt = output_hdr ? AV_PIX_FMT_YUV420P10LE : AV_PIX_FMT_YUV420P;

    // Encoder time base = 1 / frame_rate
    const AVRational fr = in_video->avg_frame_rate;
    enc_ctx->time_base = (fr.num > 0)
        ? AVRational{ fr.den, fr.num }
        : AVRational{ 1, 25 };
    enc_ctx->framerate = fr;

    // Colour metadata
    if (output_hdr) {
        enc_ctx->color_primaries = AVCOL_PRI_BT2020;
        enc_ctx->color_trc       = AVCOL_TRC_SMPTE2084;
        enc_ctx->colorspace      = AVCOL_SPC_BT2020_NCL;
    } else {
        enc_ctx->color_primaries = AVCOL_PRI_BT709;
        enc_ctx->color_trc       = AVCOL_TRC_BT709;
        enc_ctx->colorspace      = AVCOL_SPC_BT709;
    }
    enc_ctx->color_range = AVCOL_RANGE_MPEG; // limited range for both

    // MKV/MP4 require the sequence header in extradata, not in the bitstream
    if (ofmt->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // SVT-AV1 options
    AVDictionary* enc_opts = nullptr;
    av_dict_set_int(&enc_opts, "preset", cfg.preset, 0);
    if (cfg.crf > 0)
        av_dict_set_int(&enc_opts, "crf", cfg.crf, 0);

    const int enc_ret = avcodec_open2(enc_ctx.get(), encoder, &enc_opts);
    av_dict_free(&enc_opts);
    if (enc_ret < 0)
        throw std::runtime_error("Failed to open encoder");

    // Add video stream
    AVStream* out_video = avformat_new_stream(ofmt.get(), nullptr);
    if (!out_video)
        throw std::runtime_error("Failed to create output video stream");

    if (avcodec_parameters_from_context(out_video->codecpar, enc_ctx.get()) < 0)
        throw std::runtime_error("Failed to copy encoder params to output stream");
    out_video->time_base = enc_ctx->time_base;

    // Pass-through audio streams (copy without re-encoding)
    // Map: input stream index → output stream index
    std::vector<int> stream_map(ifmt->nb_streams, -1);
    stream_map[video_idx] = out_video->index;

    for (unsigned i = 0; i < ifmt->nb_streams; ++i) {
        if ((int)i == video_idx) continue;
        AVStream* in_s = ifmt->streams[i];
        if (in_s->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_s->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE)
            continue;

        AVStream* out_s = avformat_new_stream(ofmt.get(), nullptr);
        if (!out_s) continue;
        if (avcodec_parameters_copy(out_s->codecpar, in_s->codecpar) < 0) continue;
        out_s->codecpar->codec_tag = 0;
        out_s->time_base = in_s->time_base;
        stream_map[i] = out_s->index;
    }

    // Open output file
    if (!(ofmt->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt->pb, output.c_str(), AVIO_FLAG_WRITE) < 0)
            throw std::runtime_error("Cannot open output file: " + output);
    }

    if (avformat_write_header(ofmt.get(), nullptr) < 0)
        throw std::runtime_error("Failed to write output header");

    // --- Allocate frames / packet ---
    FramePtr dec_frame(av_frame_alloc());
    FramePtr enc_frame(av_frame_alloc());
    PacketPtr pkt(av_packet_alloc());
    if (!dec_frame || !enc_frame || !pkt)
        throw std::runtime_error("Failed to allocate frame/packet");

    // Pre-allocate encoder frame buffer (only needed for SDR conversion path)
    if (!output_hdr) {
        enc_frame->width  = enc_ctx->width;
        enc_frame->height = enc_ctx->height;
        enc_frame->format = (int)AV_PIX_FMT_YUV420P;
        if (av_frame_get_buffer(enc_frame.get(), 0) < 0)
            throw std::runtime_error("Failed to alloc SDR output frame buffer");
    }

    int64_t frame_count = 0;

    // Sends one frame (or nullptr for flush) to the encoder and writes output packets
    auto drain_encoder = [&](AVFrame* frame) {
        // EAGAIN means the encoder output buffer is full — drain packets first, then retry
        int sf_ret = avcodec_send_frame(enc_ctx.get(), frame);
        if (sf_ret == AVERROR(EAGAIN)) {
            while (true) {
                const int r = avcodec_receive_packet(enc_ctx.get(), pkt.get());
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
                if (r < 0) throw std::runtime_error("Error draining encoder before retry");
                av_packet_rescale_ts(pkt.get(), enc_ctx->time_base, out_video->time_base);
                pkt->stream_index = out_video->index;
                const int wr = av_interleaved_write_frame(ofmt.get(), pkt.get());
                if (wr < 0) {
                    char errbuf[256];
                    av_strerror(wr, errbuf, sizeof(errbuf));
                    throw std::runtime_error(std::string("Error writing video packet: ") + errbuf);
                }
            }
            sf_ret = avcodec_send_frame(enc_ctx.get(), frame);
        }
        if (sf_ret < 0)
            throw std::runtime_error("Error sending frame to encoder");

        while (true) {
            const int r = avcodec_receive_packet(enc_ctx.get(), pkt.get());
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
            if (r < 0) throw std::runtime_error("Error receiving packet from encoder");

            av_packet_rescale_ts(pkt.get(), enc_ctx->time_base, out_video->time_base);
            pkt->stream_index = out_video->index;

            const int wr = av_interleaved_write_frame(ofmt.get(), pkt.get());
            if (wr < 0) {
                char errbuf[256];
                av_strerror(wr, errbuf, sizeof(errbuf));
                throw std::runtime_error(std::string("Error writing video packet: ") + errbuf);
            }
        }
    };

    // Process one decoded frame
    auto process_and_encode = [&](AVFrame* df) {
        AVFrame* send_frame = nullptr;

        if (output_hdr) {
            // FIX 2: verify the decoded frame is actually 10-bit before passthrough
            if (df->format != AV_PIX_FMT_YUV420P10LE)
                throw std::runtime_error("HDR passthrough requires yuv420p10le input; "
                                          "got pixel format " + std::to_string(df->format));

            df->color_primaries = AVCOL_PRI_BT2020;
            df->color_trc       = AVCOL_TRC_SMPTE2084;
            df->colorspace      = AVCOL_SPC_BT2020_NCL;
            df->color_range     = AVCOL_RANGE_MPEG;
            send_frame = df;
        } else {
            // SDR: apply colour conversion into enc_frame
            if (av_frame_make_writable(enc_frame.get()) < 0)
                throw std::runtime_error("Failed to make encoder frame writable");

            process_frame_sdr(df, enc_frame.get(), cfg.peak_nits);
            enc_frame->color_primaries = AVCOL_PRI_BT709;
            enc_frame->color_trc       = AVCOL_TRC_BT709;
            enc_frame->colorspace      = AVCOL_SPC_BT709;
            enc_frame->color_range     = AVCOL_RANGE_MPEG;
            send_frame = enc_frame.get();
        }

        // Rescale PTS from decoder pkt_timebase to encoder time_base
        const int64_t src_pts = (df->best_effort_timestamp != AV_NOPTS_VALUE)
            ? df->best_effort_timestamp
            : df->pts;
        send_frame->pts = (src_pts != AV_NOPTS_VALUE)
            ? av_rescale_q(src_pts, dec_ctx->pkt_timebase, enc_ctx->time_base)
            : frame_count;

        ++frame_count;
        drain_encoder(send_frame);
    };

    // FIX 7: on any exception after the output file is open, delete the partial file
    try {
        // --- Main decode loop ---
        while (true) {
            const int read_ret = av_read_frame(ifmt.get(), pkt.get());
            if (read_ret == AVERROR_EOF) break;
            if (read_ret < 0) throw std::runtime_error("Error reading input frame");

            if (pkt->stream_index == video_idx) {
                // FIX 1: EAGAIN means decoder buffer is full — drain frames then retry
                int send_ret = avcodec_send_packet(dec_ctx.get(), pkt.get());
                if (send_ret == AVERROR(EAGAIN)) {
                    while (true) {
                        const int recv_ret = avcodec_receive_frame(dec_ctx.get(), dec_frame.get());
                        if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) break;
                        if (recv_ret < 0) {
                            av_packet_unref(pkt.get());
                            throw std::runtime_error("Error receiving decoded frame during EAGAIN drain");
                        }
                        process_and_encode(dec_frame.get());
                        av_frame_unref(dec_frame.get());
                    }
                    send_ret = avcodec_send_packet(dec_ctx.get(), pkt.get());
                }
                av_packet_unref(pkt.get());
                if (send_ret < 0)
                    throw std::runtime_error("Error sending packet to decoder");

                while (true) {
                    const int recv_ret = avcodec_receive_frame(dec_ctx.get(), dec_frame.get());
                    if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) break;
                    if (recv_ret < 0) throw std::runtime_error("Error receiving decoded frame");

                    process_and_encode(dec_frame.get());
                    av_frame_unref(dec_frame.get());
                }
            } else {
                // Audio / subtitle pass-through
                const int out_idx = (pkt->stream_index < (int)stream_map.size())
                    ? stream_map[pkt->stream_index] : -1;
                if (out_idx >= 0) {
                    AVStream* in_s  = ifmt->streams[pkt->stream_index];
                    AVStream* out_s = ofmt->streams[out_idx];
                    av_packet_rescale_ts(pkt.get(), in_s->time_base, out_s->time_base);
                    pkt->stream_index = out_idx;
                    // FIX 6: check audio write errors
                    const int aw = av_interleaved_write_frame(ofmt.get(), pkt.get());
                    if (aw < 0) {
                        char errbuf[256];
                        av_strerror(aw, errbuf, sizeof(errbuf));
                        throw std::runtime_error(std::string("Error writing audio packet: ") + errbuf);
                    }
                } else {
                    av_packet_unref(pkt.get());
                }
            }
        }

        // --- Flush decoder ---
        avcodec_send_packet(dec_ctx.get(), nullptr);
        while (avcodec_receive_frame(dec_ctx.get(), dec_frame.get()) >= 0) {
            process_and_encode(dec_frame.get());
            av_frame_unref(dec_frame.get());
        }

        // --- Flush encoder ---
        drain_encoder(nullptr);

        if (av_write_trailer(ofmt.get()) < 0)
            throw std::runtime_error("Failed to write output trailer (disk full?)");

    } catch (...) {
        // FIX 7: release FFmpeg handles before deleting the file,
        // so the file descriptor is closed first on all platforms
        ofmt.reset();
        std::remove(output.c_str());
        throw;
    }

    std::cout << "Done. " << frame_count << " frames encoded.\n";
}

} // namespace encode
