# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Same-Blue** is a cross-platform C++20 HDR video probe and AV1 encoder. It probes HDR metadata from video files (via FFmpeg) and encodes to AV1 using SVT-AV1, with full HDR→SDR color pipeline support. Architecture: C++ binary for the hot path, Go CLI for orchestration and user interface.

**Ultimate goal:** real-time HDR game capture on Windows (DXGI Desktop Duplication), process frames, encode to AV1 — supporting both HDR (BT.2020 PQ 10-bit) and SDR (BT.709 8-bit with tone mapping) output.

**User profile:** the primary contributor is a Go developer, not a C++ developer. Keep Go-side code approachable and use Go idioms. C++ changes require more care and explicit review.

## Build

```sh
brew install ffmpeg          # macOS only
cmake -B build -S .
cmake --build build
go build ./cmd/same-blue
```

```sh
./build/Same_Blue_tests      # C++ unit tests (53 checks)
go test ./...                # Go tests (22 tests across 2 packages)
```

CMake auto-detects the platform and links the appropriate backend. On macOS it uses `pkg-config` to locate FFmpeg from `/opt/homebrew` or `/usr/local`.

## Architecture

### C++/Go boundary

The C++ binary (`Same_Blue`) and the Go CLI communicate through subprocess only — no shared memory, no CGo, no HTTP. The Go `runner.Runner` spawns `Same_Blue` as a child process and reads its output:

- **Probe path:** `Same_Blue --json <file>` → JSON on stdout, diagnostics on stderr
- **Transcode path:** `Same_Blue --transcode <in> <out> [flags]` → progress on stdout/stderr

This boundary is intentional. It means C++ and Go can evolve independently, and the Go layer is always testable without a real binary present.

### C++ source layout (`src/`)

- `hdr/color_spaces.h` — CICP enums (`ColorPrimaries`, `TransferFunction`, `ColorSpace`) and structs (`MasteringDisplay`, `ContentLightLevel`). Every other file includes this. It is the shared vocabulary — be conservative about changing it.
- `hdr/avformat_raii.h` — RAII wrapper for FFmpeg `AVFormatContext`. Move-only; throws `std::runtime_error` on failure. `open()` only updates internal state after both `avformat_open_input` and `avformat_find_stream_info` succeed.
- `hdr/hdr_probe.h` / `hdr_probe.cpp` — `HDRProbe` class: opens a file, extracts CICP values and HDR10 side-data (mastering display, MaxCLL/MaxFALL) from the first video stream.
- `process/color_convert.h` — header-only color math: PQ EOTF/OETF, BT.2020→BT.709 matrix, Reinhard tone mapping, YCbCr→RGB for BT.2020 and BT.709, full `HDR10_to_SDR()` pipeline.
- `encode/pipeline.h` / `pipeline.cpp` — `encode::transcode()`: FFmpeg decode → color convert → SVT-AV1 encode. `encode::Config` holds mode/preset/crf/peak_nits.
- `platform/macos_metal.cpp` — macOS Metal stub (rendering, not yet used in encode path).
- `platform/win32_dx11.cpp` — Windows DirectX 11 stub (empty, next phase).
- `main.cpp` — entry point. Routes: `--json <file>` → probe JSON; `<file>` → probe text; `--transcode <in> <out> [flags]` → encode.
- `tests/color_convert_test.cpp` — standalone C++ test binary, no framework dependency.

### Go source layout

- `cmd/same-blue/main.go` — CLI entry point. Parses flags with `flag` stdlib, calls `runner.Runner`.
- `internal/runner/runner.go` — spawns the C++ binary. `TranscodeConfig.Preset` and `CRF` are `*int` (pointer) so nil means "omit flag, use C++ default" and `0` is a valid value (preset 0 = max quality SVT-AV1).
- `internal/probe/probe.go` — typed CICP constants (`ColorPrimaries`, `TransferCharacteristics`, `MatrixCoefficients`) with `String()` methods mirroring C++ `to_string()`. `Parse()` uses `json.Unmarshal`.

## Critical Takeaways and Known Issues

### Architectural decisions worth preserving

**CICP integers in JSON, not strings.** The probe JSON uses integer values (e.g. `"color_primaries": 9`) not display strings (e.g. `"BT.2020"`). This was deliberate: C++ `to_string()` output format can change freely without breaking Go parsing. The Go typed constants (`ColorPrimariesBT2020 = 9`) own the display logic on the Go side.

**`*int` for Preset and CRF in TranscodeConfig.** SVT-AV1 preset 0 is the highest quality setting — a valid and meaningful value. Using `int` with `0` as "unset" sentinel would silently swallow the most-quality preset. `*int` where nil = omit flag is the correct pattern here.

**Stdout/stderr separation in Probe().** `cmd.CombinedOutput()` was the original implementation. This was wrong: FFmpeg writes diagnostic lines to stderr (e.g. `[matroska,webm @ 0x...] Stream #0: not enough frames to estimate rate`). If combined, these lines would corrupt JSON parsing. The fix uses separate `bytes.Buffer` for stdout and stderr.

**`probe_file_json()` routing must come before `argc == 2` check.** `--json <file>` is argc=3. If the `argc == 2` check appeared first, `--json` would never match. The order in `main()` is load-bearing.

### Known gaps to address before shipping

**SMPTE ST.2086 side data is not re-embedded in transcoded output.** When you probe a transcoded file with `-mode hdr`, it shows `HDR metadata: no` even though the source had mastering display metadata. The color space (BT.2020/PQ) is correctly signaled, but the MDCV SEI block (mastering display chromaticity + luminance) is not written to the output container. This requires copying `AVPacketSideData` of type `AV_PKT_DATA_MASTERING_DISPLAY_METADATA` and `AV_PKT_DATA_CONTENT_LIGHT_LEVEL` from the input stream to the output stream in `pipeline.cpp`. This is the most visible remaining gap for HDR fidelity.

**`test_hdr_components()` runs on invalid arguments.** The `else` branch in `main()` (when no recognized arguments are given) calls `test_hdr_components()` — a development scaffolding function that creates test objects and prints to stdout. This will surprise users who mistype a command. It should eventually be removed or moved behind a `--self-test` flag.

**FFmpeg stderr diagnostics are expected for short MKV files.** When probing transcoded `.mkv` files with short duration, FFmpeg emits `Stream #0: not enough frames to estimate rate; consider increasing probesize` to stderr. This is benign — it does not affect JSON output — but it appears in terminal output during transcode and may confuse users. It can be suppressed by setting `AVFormatContext.probesize` larger before opening.

**MasteringDisplay primary ordering in C++.** `display_primaries_x[0]` = Green, `[1]` = Blue, `[2]` = Red — this is FFmpeg's `AVMasteringDisplayMetadata` ordering (G/B/R), not the conventional display order (R/G/B). The JSON output correctly labels each channel but this ordering is easy to get wrong when writing new C++ code that reads from or writes to this struct.

**`has_hdr_metadata` means container side-data, not color space.** `is_hdr_content` is true when the transfer function is PQ or HLG. `has_hdr_metadata` is true only when SMPTE ST.2086 mastering display data is present as side-data in the container. A file can be HDR content without HDR metadata (our transcoded output is exactly this case). Do not conflate the two.

**Windows build is a stub.** `src/platform/win32_dx11.cpp` is 41 bytes (a comment). The CMake Windows path compiles it but does nothing useful. The entire DXGI capture + DirectX 11 rendering pipeline is future work.

### Things that were fixed and why — don't revert

**`probe_file()` returns bool, not `std::exit()`.** The original used `std::exit(1)` on error which bypasses RAII destructors and leaves FFmpeg handles open. It now returns `false` and lets `main()` return the exit code.

**`flag.Args()[1:]` for subcommand args.** `os.Args[1+flag.NFlag()+1:]` was wrong because `flag.NFlag()` counts flags set, not tokens consumed. A two-token flag like `--binary /path` increments `NFlag` by 1 but consumes 2 tokens from `os.Args`. `flag.Args()` is the correct post-parse remainder.

**`parseLuminance` must `TrimSpace` before `TrimPrefix`.** Probe output lines have leading whitespace (`  Luminance: ...`). The original `TrimPrefix("Luminance:")` silently failed on such lines, returning `(0, 0)`. Fixed by `TrimSpace` first. (Now moot since probe is JSON-based, but the pattern matters.)

**Integration test uses `t.Error`, not `t.Logf`.** The original used `t.Logf` which is informational only — the test would pass even when the binary failed. `t.Error` correctly marks the test as failed.

## Key Standards Reference

- **CICP color primaries** (ISO/IEC 23091-2): BT.709=1, BT.2020=9, DCI-P3=11, Display P3=12
- **CICP transfer characteristics**: BT.709=1, PQ=16, HLG=18
- **CICP matrix coefficients**: BT.709=1, BT.2020 NCL=9, BT.2020 CL=10
- **PQ (SMPTE ST.2084)**: maps [0,1] linear to [0, 10000 nits]; `LinearToPQ(1.0)` = 10000 nits full scale
- **YCbCr limited range (10-bit)**: Y in [64, 940], Cb/Cr in [64, 960], neutral chroma at 512
- **YCbCr limited range (8-bit)**: Y in [16, 235], Cb/Cr in [16, 240], neutral chroma at 128
- **SVT-AV1 preset**: 0 = slowest/best quality, 12 = fastest/worst quality — preset 0 is a valid production choice, not a zero sentinel
