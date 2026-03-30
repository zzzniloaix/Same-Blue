#pragma once

#include <cmath>
#include <algorithm>

namespace process {

// ---------------------------------------------------------------------------
// Vec3 — linear RGB in scene-referred light (0.0–1.0, where 1.0 = 10 000 nits
// for PQ-encoded signals)
// ---------------------------------------------------------------------------
struct Vec3 {
    double r, g, b;

    Vec3 operator+(const Vec3& o) const noexcept { return {r+o.r, g+o.g, b+o.b}; }
    Vec3 operator*(double s)       const noexcept { return {r*s,   g*s,   b*s};   }
    Vec3 clamp(double lo, double hi) const noexcept {
        return {
            std::clamp(r, lo, hi),
            std::clamp(g, lo, hi),
            std::clamp(b, lo, hi)
        };
    }
};

// ---------------------------------------------------------------------------
// 3×3 matrix multiply — row-major: m[row][col]
// ---------------------------------------------------------------------------
inline Vec3 mat3_mul(const double m[3][3], Vec3 v) noexcept {
    return {
        m[0][0]*v.r + m[0][1]*v.g + m[0][2]*v.b,
        m[1][0]*v.r + m[1][1]*v.g + m[1][2]*v.b,
        m[2][0]*v.r + m[2][1]*v.g + m[2][2]*v.b
    };
}

// ---------------------------------------------------------------------------
// SMPTE ST.2084 (PQ) — constants shared with HDRImage in main.cpp
// ---------------------------------------------------------------------------
namespace pq {
    inline constexpr double m1 = 2610.0 / 4096.0 / 4.0;
    inline constexpr double m2 = 2523.0 / 4096.0 * 128.0;
    inline constexpr double c1 = 3424.0 / 4096.0;
    inline constexpr double c2 = 2413.0 / 4096.0 * 32.0;
    inline constexpr double c3 = 2392.0 / 4096.0 * 32.0;
}

// PQ OETF  : linear [0,1] (1.0 = 10 000 nits) → normalized PQ [0,1]
inline double LinearToPQ(double L) noexcept {
    if (L <= 0.0) return 0.0;
    const double Lp  = std::pow(L, pq::m1);
    const double num = pq::c1 + pq::c2 * Lp;
    const double den = 1.0  + pq::c3 * Lp;
    return std::pow(num / den, pq::m2);
}

// PQ EOTF  : normalized PQ [0,1] → linear [0,1] (1.0 = 10 000 nits)
inline double PQToLinear(double E) noexcept {
    if (E <= 0.0) return 0.0;
    if (E >= 1.0) return 1.0;  // FIX 4: clamp before pow — E>1 makes den negative → NaN
    const double Ep  = std::pow(E, 1.0 / pq::m2);
    const double num = std::max(0.0, Ep - pq::c1);
    const double den = pq::c2 - pq::c3 * Ep;  // guaranteed > 0 for E ∈ (0,1)
    return std::pow(num / den, 1.0 / pq::m1);
}

// ---------------------------------------------------------------------------
// YCbCr → linear RGB
//
// Input  : 10-bit limited-range values (HDR10 default)
//            Y  ∈ [64, 940]   Cb/Cr ∈ [64, 960]  (10-bit)
//          Or 8-bit limited-range (SDR)
//            Y  ∈ [16, 235]   Cb/Cr ∈ [16, 240]  (8-bit)
//
// Output : RGB in [0, 1] (still transfer-function encoded — PQ or BT.709)
// ---------------------------------------------------------------------------

// BT.2020 NCL  (Kr=0.2627, Kb=0.0593)
inline Vec3 YCbCr_BT2020_to_RGB(int Y, int Cb, int Cr, bool bit10 = true) noexcept {
    const double y_scale  = bit10 ? 876.0 : 219.0;
    const double c_scale  = bit10 ? 896.0 : 224.0;
    const double y_offset = bit10 ?  64.0 :  16.0;
    const double c_mid    = bit10 ? 512.0 : 128.0;

    const double y  = (Y  - y_offset) / y_scale;
    const double cb = (Cb - c_mid)    / c_scale;
    const double cr = (Cr - c_mid)    / c_scale;

    return {
        y + 1.4746  * cr,
        y - 0.16450 * cb - 0.57174 * cr,
        y + 1.8814  * cb
    };
}

// BT.709  (Kr=0.2126, Kb=0.0722)
inline Vec3 YCbCr_BT709_to_RGB(int Y, int Cb, int Cr, bool bit10 = false) noexcept {
    const double y_scale  = bit10 ? 876.0 : 219.0;
    const double c_scale  = bit10 ? 896.0 : 224.0;
    const double y_offset = bit10 ?  64.0 :  16.0;
    const double c_mid    = bit10 ? 512.0 : 128.0;

    const double y  = (Y  - y_offset) / y_scale;
    const double cb = (Cb - c_mid)    / c_scale;
    const double cr = (Cr - c_mid)    / c_scale;

    return {
        y + 1.5748  * cr,
        y - 0.18732 * cb - 0.46812 * cr,
        y + 1.8556  * cb
    };
}

// ---------------------------------------------------------------------------
// Color primaries conversion: BT.2020 → BT.709
// Source: XYZ intermediary using BT.2020 and BT.709 primary chromaticities
// (both with D65 white point — no chromatic adaptation needed)
// Row sums ≈ 1, confirming D65 white is preserved.
// ---------------------------------------------------------------------------
inline constexpr double k_bt2020_to_bt709[3][3] = {
    {  1.6605, -0.5876, -0.0728 },
    { -0.1246,  1.1329, -0.0083 },
    { -0.0182, -0.1006,  1.1187 }
};

inline Vec3 BT2020_to_BT709(Vec3 rgb) noexcept {
    return mat3_mul(k_bt2020_to_bt709, rgb);
}

// ---------------------------------------------------------------------------
// BT.709 OETF: linear [0,1] → gamma-encoded [0,1]
// ---------------------------------------------------------------------------
inline double LinearToBT709(double L) noexcept {
    if (L < 0.0)      return 0.0;
    if (L < 0.018054) return 4.5 * L;
    return 1.0993 * std::pow(L, 0.45) - 0.0993;
}

inline Vec3 LinearToBT709(Vec3 v) noexcept {
    return { LinearToBT709(v.r), LinearToBT709(v.g), LinearToBT709(v.b) };
}

// ---------------------------------------------------------------------------
// Tone mapping
//
// Input  : linear BT.2020 RGB, [0,1] where 1.0 = 10 000 nits
// Output : linear BT.2020 RGB scaled to SDR range (peak_nits → 100 nits)
//
// Uses extended Reinhard on luminance to preserve hue.
// peak_nits: the mastering display peak (e.g. 1000 nits). Default 1000.
// ---------------------------------------------------------------------------
inline double tone_map_reinhard(double L, double peak_nits) noexcept {
    if (peak_nits <= 0.0) return 0.0;  // FIX 8: guard divide-by-zero
    // Normalise so peak_nits → 1.0
    const double Ln = L * 10000.0 / peak_nits;
    // Modified Reinhard curve: Ln=1 → 1.0, Ln>1 clips to 1.0
    return std::min(1.0, Ln * (1.0 + Ln) / (1.0 + Ln * Ln));
}

inline Vec3 ToneMapReinhard(Vec3 linear_bt2020, double peak_nits = 1000.0) noexcept {
    // Work on luminance (BT.2020 coefficients) to preserve hue
    const double luma = 0.2627 * linear_bt2020.r
                      + 0.6780 * linear_bt2020.g
                      + 0.0593 * linear_bt2020.b;
    if (luma < 1e-10) return {0.0, 0.0, 0.0};  // FIX 3: near-zero guard avoids division by subnormal

    const double luma_mapped = tone_map_reinhard(luma, peak_nits);
    const double scale = luma_mapped / luma;
    return (linear_bt2020 * scale).clamp(0.0, 1.0);
}

// ---------------------------------------------------------------------------
// BT.709 gamma-encoded RGB [0,1] → 8-bit limited-range YCbCr
// Y  ∈ [16, 235], Cb/Cr ∈ [16, 240]
// ---------------------------------------------------------------------------
struct YCbCr8 { uint8_t y, cb, cr; };

inline YCbCr8 RGB_BT709_to_YCbCr8(Vec3 rgb) noexcept {
    const double Y  =  0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
    const double Cb = (rgb.b - Y) / 1.8556;
    const double Cr = (rgb.r - Y) / 1.5748;
    return {
        static_cast<uint8_t>(std::clamp(std::round(Y  * 219.0 + 16.0),  16.0, 235.0)),
        static_cast<uint8_t>(std::clamp(std::round(Cb * 224.0 + 128.0), 16.0, 240.0)),
        static_cast<uint8_t>(std::clamp(std::round(Cr * 224.0 + 128.0), 16.0, 240.0))
    };
}

// ---------------------------------------------------------------------------
// Full pipelines
// ---------------------------------------------------------------------------

// HDR10 (10-bit limited YCbCr, PQ, BT.2020)
//   → SDR (linear BT.709, ready for BT.709 OETF encoding)
// peak_nits: mastering display peak from HDR metadata (use 1000 if unknown)
inline Vec3 HDR10_to_SDR(int Y, int Cb, int Cr, double peak_nits = 1000.0) noexcept {
    // 1. YCbCr → PQ-encoded RGB (BT.2020, still in PQ)
    Vec3 pq_rgb = YCbCr_BT2020_to_RGB(Y, Cb, Cr, /*bit10=*/true);

    // 2. PQ EOTF: PQ-encoded → linear light (0–1, 1.0 = 10 000 nits)
    Vec3 linear = {
        PQToLinear(pq_rgb.r),
        PQToLinear(pq_rgb.g),
        PQToLinear(pq_rgb.b)
    };

    // 3. Tone map HDR → SDR luminance range
    Vec3 tm = ToneMapReinhard(linear, peak_nits);

    // 4. BT.2020 → BT.709 color primaries (still linear)
    Vec3 bt709_linear = BT2020_to_BT709(tm).clamp(0.0, 1.0);

    return bt709_linear;
}

// HDR10 passthrough: verify the signal is truly HDR, return linear BT.2020
inline Vec3 HDR10_to_linear_BT2020(int Y, int Cb, int Cr) noexcept {
    Vec3 pq_rgb = YCbCr_BT2020_to_RGB(Y, Cb, Cr, /*bit10=*/true);
    return {
        PQToLinear(pq_rgb.r),
        PQToLinear(pq_rgb.g),
        PQToLinear(pq_rgb.b)
    };
}

} // namespace process
