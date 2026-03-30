//
// Unit tests for src/process/color_convert.h
// Build target: Same_Blue_tests
// Run:  ./build/Same_Blue_tests
//

#include "../process/color_convert.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int g_failed = 0;
static int g_passed = 0;

static void check(bool ok, const char* expr, const char* file, int line) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("  FAIL  %s:%d  %s\n", file, line, expr);
    }
}

#define CHECK(expr)           check((expr), #expr, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) check(std::abs((a)-(b)) < (eps), \
    (std::string(#a " ≈ " #b " (eps=") + std::to_string(eps) + ")").c_str(), __FILE__, __LINE__)

// ---------------------------------------------------------------------------
// PQ transfer function
// ---------------------------------------------------------------------------
static void test_pq_roundtrip() {
    std::printf("PQ round-trip\n");
    for (double L : {0.0, 0.001, 0.01, 0.1, 0.5, 1.0}) {
        double recovered = process::PQToLinear(process::LinearToPQ(L));
        CHECK_NEAR(recovered, L, 1e-9);
    }
}

static void test_pq_boundary() {
    std::printf("PQ boundary conditions\n");
    // LinearToPQ: zero and negative → 0
    CHECK_NEAR(process::LinearToPQ(0.0),  0.0, 1e-15);
    CHECK_NEAR(process::LinearToPQ(-1.0), 0.0, 1e-15);

    // PQToLinear: zero → 0, ≥1 → 1 (no NaN for out-of-range inputs)
    CHECK_NEAR(process::PQToLinear(0.0),  0.0, 1e-15);
    CHECK_NEAR(process::PQToLinear(1.0),  1.0, 1e-15);
    CHECK_NEAR(process::PQToLinear(2.0),  1.0, 1e-15);   // FIX 4: was NaN before
    CHECK_NEAR(process::PQToLinear(-0.5), 0.0, 1e-15);

    // 1.0 → 10 000 nits (full scale)
    CHECK_NEAR(process::LinearToPQ(1.0), 1.0, 1e-9);
}

// ---------------------------------------------------------------------------
// BT.2020 → BT.709 matrix
// ---------------------------------------------------------------------------
static void test_bt2020_to_bt709_white() {
    std::printf("BT.2020→BT.709: D65 white preservation\n");
    process::Vec3 white = process::BT2020_to_BT709({1.0, 1.0, 1.0});
    // Row sums ≈ 1: white should stay white to within rounding
    CHECK_NEAR(white.r, 1.0, 1e-3);
    CHECK_NEAR(white.g, 1.0, 1e-3);
    CHECK_NEAR(white.b, 1.0, 1e-3);
}

static void test_bt2020_to_bt709_black() {
    std::printf("BT.2020→BT.709: black stays black\n");
    process::Vec3 black = process::BT2020_to_BT709({0.0, 0.0, 0.0});
    CHECK_NEAR(black.r, 0.0, 1e-15);
    CHECK_NEAR(black.g, 0.0, 1e-15);
    CHECK_NEAR(black.b, 0.0, 1e-15);
}

// ---------------------------------------------------------------------------
// Tone mapping
// ---------------------------------------------------------------------------
static void test_tone_map_peak() {
    std::printf("Tone mapping: peak and boundary\n");
    // Input at exactly peak_nits → maps to 1.0
    // Ln = peak_nits * 10000 / peak_nits = 10000... wait, linear L is in [0,1] where 1=10000 nits
    // peak_nits=1000 → Ln = 1.0 * 10000 / 1000 = 10 → > 1, clips
    // To hit exactly 1.0 in Ln, need L = peak_nits / 10000
    const double peak = 1000.0;
    // L = peak/10000 → Ln = 1.0 → Reinhard: 1*(1+1)/(1+1²) = 1.0
    const double L_at_peak = peak / 10000.0;
    CHECK_NEAR(process::tone_map_reinhard(L_at_peak, peak), 1.0, 1e-9);

    // Zero input → zero output
    CHECK_NEAR(process::tone_map_reinhard(0.0, peak), 0.0, 1e-15);

    // FIX 8: peak_nits=0 → 0 (no divide-by-zero)
    CHECK_NEAR(process::tone_map_reinhard(0.5, 0.0), 0.0, 1e-15);

    // Input well above peak → clipped to 1.0
    CHECK_NEAR(process::tone_map_reinhard(1.0, peak), 1.0, 1e-9);
}

static void test_tone_map_reinhard_hue() {
    std::printf("ToneMapReinhard: hue preservation (scale applied uniformly)\n");
    // Pure red at half peak → all channels scaled equally
    const process::Vec3 red = {0.05, 0.0, 0.0};
    process::Vec3 out = process::ToneMapReinhard(red, 1000.0);
    // Green and blue must remain zero
    CHECK_NEAR(out.g, 0.0, 1e-15);
    CHECK_NEAR(out.b, 0.0, 1e-15);
    // Result must be in [0,1]
    CHECK(out.r >= 0.0 && out.r <= 1.0);
}

static void test_tone_map_near_zero_luma() {
    std::printf("ToneMapReinhard: near-zero luma guard\n");
    // FIX 3: luma < 1e-10 must return black, not NaN/inf from division
    process::Vec3 tiny = {1e-12, 1e-12, 1e-12};
    process::Vec3 out = process::ToneMapReinhard(tiny, 1000.0);
    CHECK_NEAR(out.r, 0.0, 1e-15);
    CHECK_NEAR(out.g, 0.0, 1e-15);
    CHECK_NEAR(out.b, 0.0, 1e-15);
}

// ---------------------------------------------------------------------------
// YCbCr decode
// ---------------------------------------------------------------------------
static void test_ycbcr_bt2020_grey() {
    std::printf("YCbCr BT.2020: neutral grey (Cb=Cr=512)\n");
    // Y=512 in 10-bit limited range → (512-64)/876 ≈ 0.5114 normalized
    // Cb=Cr=512 (mid) → zero chroma → R=G=B
    process::Vec3 rgb = process::YCbCr_BT2020_to_RGB(512, 512, 512, true);
    CHECK_NEAR(rgb.r, rgb.g, 1e-9);
    CHECK_NEAR(rgb.g, rgb.b, 1e-9);
    // Absolute range check: result must be within [0, 1]
    CHECK(rgb.r >= 0.0 && rgb.r <= 1.0);
    // Y=512 → approx 0.511 (limited range midpoint)
    CHECK_NEAR(rgb.r, 0.511, 0.01);

    // Y=64 (black) → 0.0
    process::Vec3 black = process::YCbCr_BT2020_to_RGB(64, 512, 512, true);
    CHECK_NEAR(black.r, 0.0, 1e-9);
    CHECK_NEAR(black.g, 0.0, 1e-9);

    // Y=940 (white) → 1.0
    process::Vec3 white = process::YCbCr_BT2020_to_RGB(940, 512, 512, true);
    CHECK_NEAR(white.r, 1.0, 1e-9);
    CHECK_NEAR(white.g, 1.0, 1e-9);
}

static void test_ycbcr_bt709_grey() {
    std::printf("YCbCr BT.709: neutral grey (Cb=Cr=128)\n");
    // Y=128 in 8-bit limited range → (128-16)/219 ≈ 0.5114
    process::Vec3 rgb = process::YCbCr_BT709_to_RGB(128, 128, 128, false);
    CHECK_NEAR(rgb.r, rgb.g, 1e-9);
    CHECK_NEAR(rgb.g, rgb.b, 1e-9);
    CHECK(rgb.r >= 0.0 && rgb.r <= 1.0);
    CHECK_NEAR(rgb.r, 0.511, 0.01);

    // Y=16 (black) → 0.0
    process::Vec3 black = process::YCbCr_BT709_to_RGB(16, 128, 128, false);
    CHECK_NEAR(black.r, 0.0, 1e-9);

    // Y=235 (white) → 1.0
    process::Vec3 white = process::YCbCr_BT709_to_RGB(235, 128, 128, false);
    CHECK_NEAR(white.r, 1.0, 1e-9);
}

// ---------------------------------------------------------------------------
// BT.709 OETF
// ---------------------------------------------------------------------------
static void test_bt709_oetf() {
    std::printf("BT.709 OETF\n");
    // Black → 0
    CHECK_NEAR(process::LinearToBT709(0.0), 0.0, 1e-15);
    // Negative → 0
    CHECK_NEAR(process::LinearToBT709(-0.1), 0.0, 1e-15);
    // Linear segment boundary
    CHECK_NEAR(process::LinearToBT709(0.018054), 4.5 * 0.018054, 1e-4);
    // White (1.0) → ~1.0
    CHECK_NEAR(process::LinearToBT709(1.0), 1.0, 1e-3);
}

// ---------------------------------------------------------------------------
// Full HDR10→SDR pipeline
// ---------------------------------------------------------------------------
static void test_hdr10_to_sdr_midgrey() {
    std::printf("HDR10→SDR pipeline: mid-grey\n");
    // Y=512, Cb=Cr=512 → neutral grey, output should be neutral
    process::Vec3 sdr = process::HDR10_to_SDR(512, 512, 512, 1000.0);
    CHECK_NEAR(sdr.r, sdr.g, 1e-3);
    CHECK_NEAR(sdr.g, sdr.b, 1e-3);
    CHECK(sdr.r >= 0.0 && sdr.r <= 1.0);
}

static void test_hdr10_to_sdr_peak_white() {
    std::printf("HDR10→SDR pipeline: peak white clipped to 1.0\n");
    // Y=940 = full-scale PQ (10000 nits), 10× above 1000-nit peak → must clip to 1.0
    process::Vec3 sdr = process::HDR10_to_SDR(940, 512, 512, 1000.0);
    CHECK_NEAR(sdr.r, 1.0, 0.01);
    CHECK_NEAR(sdr.g, 1.0, 0.01);
    CHECK_NEAR(sdr.b, 1.0, 0.01);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    test_pq_roundtrip();
    test_pq_boundary();
    test_bt2020_to_bt709_white();
    test_bt2020_to_bt709_black();
    test_tone_map_peak();
    test_tone_map_reinhard_hue();
    test_tone_map_near_zero_luma();
    test_ycbcr_bt2020_grey();
    test_ycbcr_bt709_grey();
    test_bt709_oetf();
    test_hdr10_to_sdr_midgrey();
    test_hdr10_to_sdr_peak_white();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
