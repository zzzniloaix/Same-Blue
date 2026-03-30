#pragma once

#include "color_spaces.h"
#include "avformat_raii.h"
#include <string>
#include <optional>

namespace hdr {

/**
 * HDRProbe class for extracting color space and HDR metadata from video files
 * Uses FFmpeg to parse video stream information and HDR metadata
 */
class HDRProbe {
private:
    AVFormatContextRAII format_context_;
    int video_stream_index_ = -1;
    
    // Cached color space information
    std::optional<ColorPrimaries> color_primaries_;
    std::optional<TransferFunction> transfer_function_;
    std::optional<ColorSpace> color_space_;
    std::optional<MasteringDisplay> mastering_display_;
    std::optional<ContentLightLevel> content_light_level_;

public:
    /**
     * Default constructor
     */
    HDRProbe() = default;
    
    /**
     * Constructor that immediately loads a video file
     * @param filepath Path to video file
     * @throws std::runtime_error if file cannot be opened or contains no video streams
     */
    explicit HDRProbe(const std::string& filepath);
    
    /**
     * Load and analyze a video file
     * @param filepath Path to video file
     * @throws std::runtime_error if file cannot be opened or contains no video streams
     */
    void load_video_file(const std::string& filepath);
    
    /**
     * Get color primaries from the video stream
     * @return ColorPrimaries enum or std::nullopt if not available
     */
    std::optional<ColorPrimaries> get_color_primaries() const;
    
    /**
     * Get transfer characteristics from the video stream
     * @return TransferFunction enum or std::nullopt if not available
     */
    std::optional<TransferFunction> get_transfer_characteristics() const;
    
    /**
     * Get color space matrix coefficients from the video stream
     * @return ColorSpace enum or std::nullopt if not available
     */
    std::optional<ColorSpace> get_color_space() const;
    
    /**
     * Get HDR10 mastering display color volume metadata
     * @return MasteringDisplay struct or std::nullopt if not available
     */
    std::optional<MasteringDisplay> get_mastering_display_info() const;
    
    /**
     * Get HDR10 content light level metadata
     * @return ContentLightLevel struct or std::nullopt if not available
     */
    std::optional<ContentLightLevel> get_content_light_level() const;
    
    /**
     * Get maximum content light level (MaxCLL)
     * @return Maximum content light level in cd/m² or std::nullopt if not available
     */
    std::optional<uint16_t> get_max_content_light_level() const;
    
    /**
     * Check if the video contains HDR metadata
     * @return true if HDR10 or HDR10+ metadata is present
     */
    bool has_hdr_metadata() const;
    
    /**
     * Check if the video uses HDR transfer function (PQ or HLG)
     * @return true if transfer function is PQ or HLG
     */
    bool is_hdr_content() const;
    
    /**
     * Get video stream width
     * @return Video width in pixels or 0 if no video stream
     */
    int get_width() const;
    
    /**
     * Get video stream height
     * @return Video height in pixels or 0 if no video stream
     */
    int get_height() const;
    
    /**
     * Get video stream pixel format
     * @return AVPixelFormat or AV_PIX_FMT_NONE if no video stream
     */
    int get_pixel_format() const;
    
    /**
     * Check if a video file is currently loaded
     * @return true if video file is loaded successfully
     */
    bool is_loaded() const;

private:
    /**
     * Find the first video stream in the format context
     * @throws std::runtime_error if no video stream is found
     */
    void find_video_stream();
    
    /**
     * Parse color space information from the video stream
     */
    void parse_color_space_info();
    
    /**
     * Parse HDR metadata from the video stream
     */
    void parse_hdr_metadata();
    
    /**
     * Clear all cached information
     */
    void clear_cache();
};

} // namespace hdr