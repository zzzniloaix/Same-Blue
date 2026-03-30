#pragma once

#include <string>

namespace encode {

enum class OutputMode {
    Auto,   // HDR if input has PQ/HLG transfer, SDR otherwise
    HDR,    // Force HDR10 AV1 output (BT.2020 PQ, 10-bit)
    SDR,    // Force SDR AV1 output  (BT.709,    8-bit, tone-mapped if needed)
};

struct Config {
    OutputMode mode      = OutputMode::Auto;
    int        preset    = 8;       // SVT-AV1 preset 0 (slowest) … 12 (fastest)
    int        crf       = 35;      // quality: lower = better, 0 = use default
    double     peak_nits = 1000.0;  // mastering peak for SDR tone mapping
};

// Transcode input video → AV1 output.
// Container is inferred from the output filename extension (.mp4 / .mkv).
// Audio streams are copied without re-encoding.
// Throws std::runtime_error on any failure.
void transcode(const std::string& input,
               const std::string& output,
               const Config& cfg);

} // namespace encode
