//
// Created by Jiayu Lin on 3/19/26.
//

#include <iostream>
#include <cmath>
#include <format>

// Include our new HDR headers
#include "hdr/color_spaces.h"
#include "hdr/avformat_raii.h"
#include "hdr/hdr_probe.h"
#include "process/color_convert.h"
#include "encode/pipeline.h"


// Forward declarations
void test_hdr_components();
void test_color_math();

// Formats an std::optional<T> as a CICP integer or "null" for JSON.
template<typename T>
static std::string opt_cicp(const std::optional<T>& opt) {
    if (!opt) return "null";
    return std::to_string(static_cast<int>(*opt));
}

// Outputs a JSON object to stdout; errors go to stderr.
// Returns false on error so main() can propagate the exit code cleanly.
static bool probe_file_json(const std::string& filepath) {
    try {
        hdr::HDRProbe probe(filepath);

        std::cout << "{\n";
        std::cout << std::format("  \"width\": {},\n",             probe.get_width());
        std::cout << std::format("  \"height\": {},\n",            probe.get_height());
        std::cout << std::format("  \"color_primaries\": {},\n",   opt_cicp(probe.get_color_primaries()));
        std::cout << std::format("  \"transfer_function\": {},\n", opt_cicp(probe.get_transfer_characteristics()));
        std::cout << std::format("  \"color_space\": {},\n",       opt_cicp(probe.get_color_space()));
        std::cout << std::format("  \"is_hdr_content\": {},\n",    probe.is_hdr_content()   ? "true" : "false");
        std::cout << std::format("  \"has_hdr_metadata\": {}",     probe.has_hdr_metadata() ? "true" : "false");

        if (auto md = probe.get_mastering_display_info()) {
            std::cout << ",\n  \"mastering_display\": {\n";
            std::cout << std::format("    \"red_x\": {:.5f},\n",       md->display_primaries_x[2] / 50000.0);
            std::cout << std::format("    \"red_y\": {:.5f},\n",       md->display_primaries_y[2] / 50000.0);
            std::cout << std::format("    \"green_x\": {:.5f},\n",     md->display_primaries_x[0] / 50000.0);
            std::cout << std::format("    \"green_y\": {:.5f},\n",     md->display_primaries_y[0] / 50000.0);
            std::cout << std::format("    \"blue_x\": {:.5f},\n",      md->display_primaries_x[1] / 50000.0);
            std::cout << std::format("    \"blue_y\": {:.5f},\n",      md->display_primaries_y[1] / 50000.0);
            std::cout << std::format("    \"white_x\": {:.5f},\n",     md->white_point_x / 50000.0);
            std::cout << std::format("    \"white_y\": {:.5f},\n",     md->white_point_y / 50000.0);
            std::cout << std::format("    \"min_luminance\": {:.4f},\n", md->min_display_mastering_luminance / 10000.0);
            std::cout << std::format("    \"max_luminance\": {:.4f}\n",  md->max_display_mastering_luminance / 10000.0);
            std::cout << "  }";
        }

        if (auto cl = probe.get_content_light_level()) {
            std::cout << ",\n  \"content_light_level\": {\n";
            std::cout << std::format("    \"max_cll\": {},\n",  cl->max_content_light_level);
            std::cout << std::format("    \"max_fall\": {}\n",  cl->max_pic_average_light_level);
            std::cout << "  }";
        }

        std::cout << "\n}\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

// Returns false on error so main() can propagate the exit code cleanly.
static bool probe_file(const std::string& filepath) {
    std::cout << "\n=== Probing: " << filepath << " ===\n";
    try {
        hdr::HDRProbe probe(filepath);

        std::cout << std::format("Resolution:        {}x{}\n", probe.get_width(), probe.get_height());

        if (auto p = probe.get_color_primaries())
            std::cout << std::format("Color Primaries:   {}\n", hdr::to_string(*p));

        if (auto t = probe.get_transfer_characteristics())
            std::cout << std::format("Transfer Function: {}\n", hdr::to_string(*t));

        if (auto c = probe.get_color_space())
            std::cout << std::format("Color Space:       {}\n", hdr::to_string(*c));

        std::cout << std::format("HDR content:       {}\n", probe.is_hdr_content() ? "yes" : "no");
        std::cout << std::format("HDR metadata:      {}\n", probe.has_hdr_metadata() ? "yes" : "no");

        if (auto md = probe.get_mastering_display_info()) {
            // Primaries stored as G/B/R; convert back to float for display
            std::cout << "\nMastering Display (SMPTE 2086):\n";
            std::cout << std::format("  Red   xy:  ({:.5f}, {:.5f})\n", md->display_primaries_x[2] / 50000.0, md->display_primaries_y[2] / 50000.0);
            std::cout << std::format("  Green xy:  ({:.5f}, {:.5f})\n", md->display_primaries_x[0] / 50000.0, md->display_primaries_y[0] / 50000.0);
            std::cout << std::format("  Blue  xy:  ({:.5f}, {:.5f})\n", md->display_primaries_x[1] / 50000.0, md->display_primaries_y[1] / 50000.0);
            std::cout << std::format("  White xy:  ({:.5f}, {:.5f})\n", md->white_point_x / 50000.0, md->white_point_y / 50000.0);
            std::cout << std::format("  Luminance: {:.4f} - {:.4f} cd/m²\n",
                md->min_display_mastering_luminance / 10000.0,
                md->max_display_mastering_luminance / 10000.0);
        }

        if (auto cl = probe.get_content_light_level()) {
            std::cout << "\nContent Light Level (CTA-861.3):\n";
            std::cout << std::format("  MaxCLL:  {} cd/m²\n", cl->max_content_light_level);
            std::cout << std::format("  MaxFALL: {} cd/m²\n", cl->max_pic_average_light_level);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc == 3 && std::string(argv[1]) == "--json") {
        if (!probe_file_json(argv[2])) return 1;
    } else if (argc == 2) {
        if (!probe_file(argv[1])) return 1;
    } else if (argc >= 4 && std::string(argv[1]) == "--transcode") {
        // Usage: Same_Blue --transcode <input> <output> [--mode auto|hdr|sdr]
        //                                               [--preset N] [--crf N]
        //                                               [--peak-nits N]

        // Validate: flags must come in --flag value pairs
        if ((argc - 4) % 2 != 0) {
            std::cerr << "Error: flags must be paired (--flag value). Got odd number of option tokens.\n";
            return 1;
        }

        encode::Config cfg;

        try {
            for (int i = 4; i < argc; i += 2) {
                std::string flag = argv[i];
                std::string val  = argv[i + 1];
                if (flag == "--mode") {
                    if      (val == "hdr") cfg.mode = encode::OutputMode::HDR;
                    else if (val == "sdr") cfg.mode = encode::OutputMode::SDR;
                    else if (val == "auto") cfg.mode = encode::OutputMode::Auto;
                    else { std::cerr << "Unknown --mode value: " << val << " (expected auto|hdr|sdr)\n"; return 1; }
                } else if (flag == "--preset") {
                    cfg.preset = std::stoi(val);
                } else if (flag == "--crf") {
                    cfg.crf = std::stoi(val);
                } else if (flag == "--peak-nits") {
                    cfg.peak_nits = std::stod(val);
                } else {
                    std::cerr << "Unknown flag: " << flag << "\n";
                    return 1;
                }
            }
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: invalid numeric flag value — " << e.what() << "\n";
            return 1;
        } catch (const std::out_of_range& e) {
            std::cerr << "Error: flag value out of range — " << e.what() << "\n";
            return 1;
        }

        try {
            encode::transcode(argv[2], argv[3], cfg);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    } else {
        std::cout << "\nUsage:\n"
                  << "  Same_Blue <video>                         probe HDR metadata\n"
                  << "  Same_Blue --transcode <input> <output>    encode to AV1\n"
                  << "    [--mode auto|hdr|sdr]  output mode      (default: auto)\n"
                  << "    [--preset N]           SVT-AV1 preset   (default: 8)\n"
                  << "    [--crf N]              quality 0-63     (default: 35)\n"
                  << "    [--peak-nits N]        mastering peak   (default: 1000)\n";
        test_hdr_components();
    }

    return 0;
}
void test_color_math() {
    using namespace process;
    std::cout << "\n=== Color Math Verification ===\n";

    // 1. PQ round-trip: encode then decode should return the original value
    for (double L : {0.0, 0.01, 0.1, 0.5, 1.0}) {
        double recovered = PQToLinear(LinearToPQ(L));
        double err = std::abs(recovered - L);
        std::cout << std::format("  PQ round-trip L={:.3f}: recovered={:.6f}  err={:.2e}  {}\n",
            L, recovered, err, err < 1e-9 ? "OK" : "FAIL");
    }

    // 2. BT.2020→BT.709 matrix: D65 white (1,1,1) must map to (1,1,1)
    Vec3 white_bt2020 = {1.0, 1.0, 1.0};
    Vec3 white_bt709  = BT2020_to_BT709(white_bt2020);
    std::cout << std::format("  White preservation: R={:.6f} G={:.6f} B={:.6f}  {}\n",
        white_bt709.r, white_bt709.g, white_bt709.b,
        (std::abs(white_bt709.r - 1.0) < 1e-3 &&
         std::abs(white_bt709.g - 1.0) < 1e-3 &&
         std::abs(white_bt709.b - 1.0) < 1e-3) ? "OK" : "FAIL");

    // 3. Full HDR→SDR pipeline on a known 10-bit HDR10 pixel
    //    Y=512 Cb=512 Cr=512  →  mid-grey in limited range, near-black linear
    Vec3 sdr_mid = HDR10_to_SDR(512, 512, 512, 1000.0);
    std::cout << std::format("  HDR mid-grey→SDR: R={:.4f} G={:.4f} B={:.4f}\n",
        sdr_mid.r, sdr_mid.g, sdr_mid.b);

    //    Y=512 Cb=512 Cr=512: PQ≈0.511 → ~100 nits → ~10% of 1000-nit peak → ~0.11 linear SDR
    //    Y=940 Cb=512 Cr=512: PQ=1.0 → 10 000 nits, 10× above peak → tone-mapped/clipped to 1.0
    Vec3 sdr_peak = HDR10_to_SDR(940, 512, 512, 1000.0);
    std::cout << std::format("  HDR 10k-nit white→SDR: R={:.4f} G={:.4f} B={:.4f}  "
        "(expect ~1.0 — 10× above peak, clipped by tone mapper)\n",
        sdr_peak.r, sdr_peak.g, sdr_peak.b);
}

// Simple test function for our new components
void test_hdr_components() {
    std::cout << "\n=== Testing HDR Components ===\n";

    // Test color space enums
    auto primaries = hdr::ColorPrimaries::BT2020;
    auto transfer = hdr::TransferFunction::PQ;
    auto colorspace = hdr::ColorSpace::BT2020NCL;

    std::cout << std::format("Color Primaries: {}\n", hdr::to_string(primaries));
    std::cout << std::format("Transfer Function: {}\n", hdr::to_string(transfer));
    std::cout << std::format("Color Space: {}\n", hdr::to_string(colorspace));

    // Test AVFormatContext RAII (will work without actual video file)
    try {
        hdr::AVFormatContextRAII context;
        std::cout << "AVFormatContextRAII created successfully\n";
        std::cout << std::format("Context is open: {}\n", context.is_open());
    } catch (const std::exception& e) {
        std::cout << "AVFormatContextRAII test: " << e.what() << "\n";
    }

    // Test HDRProbe (will work without actual video file)
    try {
        hdr::HDRProbe probe;
        std::cout << "HDRProbe created successfully\n";
        std::cout << std::format("Probe is loaded: {}\n", probe.is_loaded());
    } catch (const std::exception& e) {
        std::cout << "HDRProbe test: " << e.what() << "\n";
    }
}
