#pragma once

#include <cstdint>
#include <string_view>

namespace hdr {

/**
 * ITU-T H.264/H.265 Color Primaries
 * Based on ISO/IEC 23091-2 (CICP) specifications
 */
enum class ColorPrimaries : uint8_t {
    Reserved0 = 0,
    BT709 = 1,          // Rec. ITU-R BT.709-6
    Unspecified = 2,
    Reserved3 = 3,
    BT470M = 4,         // FCC Title 47
    BT470BG = 5,        // Rec. ITU-R BT.470-6 System B, G
    SMPTE170M = 6,      // SMPTE-170M (2004)
    SMPTE240M = 7,      // SMPTE-240M (1999)
    Film = 8,           // Generic film
    BT2020 = 9,         // Rec. ITU-R BT.2020-2
    SMPTE428 = 10,      // SMPTE ST 428-1 (2019)
    SMPTE431 = 11,      // SMPTE RP 431-2 (2011)
    SMPTE432 = 12,      // SMPTE EG 432-1 (2010)
    EBU3213 = 22        // EBU Tech. 3213-E
};

/**
 * Transfer Characteristics (Gamma/OECF)
 */
enum class TransferFunction : uint8_t {
    Reserved0 = 0,
    BT709 = 1,          // Rec. ITU-R BT.709-6
    Unspecified = 2,
    Reserved3 = 3,
    Gamma22 = 4,        // Assumed display gamma 2.2
    Gamma28 = 5,        // Assumed display gamma 2.8
    SMPTE170M = 6,      // SMPTE-170M (2004)
    SMPTE240M = 7,      // SMPTE-240M (1999)
    Linear = 8,         // Linear transfer characteristics
    Log = 9,            // Logarithmic transfer (100:1 range)
    LogSqrt = 10,       // Logarithmic transfer (100*Sqrt(10):1 range)
    IEC61966_2_4 = 11,  // IEC 61966-2-4
    BT1361ECG = 12,     // ITU-R BT.1361 Extended Colour Gamut
    IEC61966_2_1 = 13,  // IEC 61966-2-1 (sRGB or sYCC)
    BT2020_10 = 14,     // ITU-R BT.2020-2 (10-bit system)
    BT2020_12 = 15,     // ITU-R BT.2020-2 (12-bit system)
    PQ = 16,            // SMPTE ST 2084 (Perceptual Quantizer)
    SMPTE428 = 17,      // SMPTE ST 428-1
    HLG = 18            // ARIB STD-B67 (Hybrid Log-Gamma)
};

/**
 * Matrix Coefficients (Color Space)
 */
enum class ColorSpace : uint8_t {
    GBR = 0,            // Identity matrix
    BT709 = 1,          // Rec. ITU-R BT.709-6
    Unspecified = 2,
    Reserved3 = 3,
    FCC = 4,            // FCC Title 47
    BT470BG = 5,        // Rec. ITU-R BT.470-6 System B, G
    SMPTE170M = 6,      // SMPTE-170M (2004)
    SMPTE240M = 7,      // SMPTE-240M (1999)
    YCGCO = 8,          // YCgCo
    BT2020NCL = 9,      // Rec. ITU-R BT.2020-2 (non-constant luminance)
    BT2020CL = 10,      // Rec. ITU-R BT.2020-2 (constant luminance)
    SMPTE2085 = 11,     // SMPTE ST 2085 (2015)
    ChromaDerivedNCL = 12,  // Chromaticity-derived non-constant luminance
    ChromaDerivedCL = 13,   // Chromaticity-derived constant luminance
    ICTCP = 14          // ITU-R BT.2100-0 ICTCP
};

/**
 * HDR Mastering Display Color Volume (MDCV)
 */
struct MasteringDisplay {
    // Display primaries in 0.00002 increments
    uint16_t display_primaries_x[3]; // Green, Blue, Red
    uint16_t display_primaries_y[3]; // Green, Blue, Red
    uint16_t white_point_x;
    uint16_t white_point_y;
    
    // Luminance in 0.0001 cd/m² increments
    uint32_t max_display_mastering_luminance;
    uint32_t min_display_mastering_luminance;
};

/**
 * HDR Content Light Level Information (CLLI)
 */
struct ContentLightLevel {
    uint16_t max_content_light_level;     // cd/m²
    uint16_t max_pic_average_light_level; // cd/m²
};

/**
 * Utility functions for enum to string conversion
 */
constexpr std::string_view to_string(ColorPrimaries primaries) noexcept {
    switch (primaries) {
        case ColorPrimaries::BT709:      return "BT.709";
        case ColorPrimaries::BT470M:     return "BT.470M";
        case ColorPrimaries::BT470BG:    return "BT.470BG";
        case ColorPrimaries::SMPTE170M:  return "SMPTE 170M";
        case ColorPrimaries::SMPTE240M:  return "SMPTE 240M";
        case ColorPrimaries::Film:       return "Generic Film";
        case ColorPrimaries::BT2020:     return "BT.2020";
        case ColorPrimaries::SMPTE428:   return "SMPTE 428";
        case ColorPrimaries::SMPTE431:   return "DCI-P3";
        case ColorPrimaries::SMPTE432:   return "Display P3";
        case ColorPrimaries::EBU3213:    return "EBU 3213";
        default:                         return "Unknown";
    }
}

constexpr std::string_view to_string(TransferFunction transfer) noexcept {
    switch (transfer) {
        case TransferFunction::BT709:       return "BT.709";
        case TransferFunction::Gamma22:     return "Gamma 2.2";
        case TransferFunction::Gamma28:     return "Gamma 2.8";
        case TransferFunction::SMPTE170M:   return "SMPTE 170M";
        case TransferFunction::SMPTE240M:   return "SMPTE 240M";
        case TransferFunction::Linear:      return "Linear";
        case TransferFunction::Log:         return "Log 100:1";
        case TransferFunction::LogSqrt:     return "Log 316:1";
        case TransferFunction::IEC61966_2_4:return "IEC 61966-2-4";
        case TransferFunction::BT1361ECG:   return "BT.1361 ECG";
        case TransferFunction::IEC61966_2_1:return "sRGB";
        case TransferFunction::BT2020_10:   return "BT.2020 10-bit";
        case TransferFunction::BT2020_12:   return "BT.2020 12-bit";
        case TransferFunction::PQ:          return "PQ (ST.2084)";
        case TransferFunction::SMPTE428:    return "SMPTE 428";
        case TransferFunction::HLG:         return "HLG";
        default:                            return "Unknown";
    }
}

constexpr std::string_view to_string(ColorSpace space) noexcept {
    switch (space) {
        case ColorSpace::GBR:              return "GBR";
        case ColorSpace::BT709:            return "BT.709";
        case ColorSpace::FCC:              return "FCC";
        case ColorSpace::BT470BG:          return "BT.470BG";
        case ColorSpace::SMPTE170M:        return "SMPTE 170M";
        case ColorSpace::SMPTE240M:        return "SMPTE 240M";
        case ColorSpace::YCGCO:            return "YCgCo";
        case ColorSpace::BT2020NCL:        return "BT.2020 NCL";
        case ColorSpace::BT2020CL:         return "BT.2020 CL";
        case ColorSpace::SMPTE2085:        return "SMPTE 2085";
        case ColorSpace::ChromaDerivedNCL: return "Chroma-derived NCL";
        case ColorSpace::ChromaDerivedCL:  return "Chroma-derived CL";
        case ColorSpace::ICTCP:            return "ICtCp";
        default:                           return "Unknown";
    }
}

} // namespace hdr