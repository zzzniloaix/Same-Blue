# Same-Blue

A cross-platform command-line tool for analyzing HDR video metadata and re-encoding video as AV1. Built as a hybrid C++/Go system: C++ handles FFmpeg decoding, color math, and SVT-AV1 encoding; Go provides the CLI and orchestration.

**Primary use case:** process HDR game footage (captured on Windows via DXGI) into standardized AV1 files — either preserving HDR (BT.2020, PQ, 10-bit) or converting to SDR (BT.709, 8-bit, Reinhard tone mapping).

> **Platform status:** macOS is the active development platform. Windows (DXGI capture) is planned.

---

## Requirements

**macOS:**
```sh
brew install ffmpeg
```

CMake 3.20+, a C++20 compiler (Apple Clang 15+ or GCC 13+), and Go 1.22+.

---

## Build

```sh
cmake -B build -S .
cmake --build build
go build ./cmd/same-blue
```

Run C++ unit tests:
```sh
./build/Same_Blue_tests
```

Run Go tests:
```sh
go test ./...
```

---

## Usage

```
same-blue — HDR video probe and AV1 encoder

Usage:
  same-blue probe <file>
  same-blue transcode [flags] <input> <output>

Commands:
  probe       Print HDR metadata for a video file
  transcode   Re-encode a video file as AV1

Transcode flags:
  -mode      string   Output mode: auto|hdr|sdr  (default "auto")
  -preset    int      SVT-AV1 preset 0–12        (default 8)
  -crf       int      Quality 0–63               (default 35)
  -peak-nits float    SDR tone mapping peak nits (default 1000)
```

---

## Examples

### Probe a file

**HDR10 source:**
```
$ same-blue probe "LG Jazz HDR UHD.mp4"

File:              LG Jazz HDR UHD.mp4
Resolution:        3840x2160
Color Primaries:   BT.2020
Transfer Function: PQ (ST.2084)
Color Space:       BT.2020 NCL
HDR content:       yes
HDR metadata:      yes

Mastering Display (SMPTE 2086):
  Red   xy:  (0.68000, 0.32000)
  Green xy:  (0.26496, 0.69000)
  Blue  xy:  (0.15000, 0.06000)
  White xy:  (0.31270, 0.32900)
  Luminance: 0.0100 – 2000.0000 cd/m²

Content Light Level (CTA-861.3):
  MaxCLL:  2000 cd/m²
  MaxFALL: 300 cd/m²
```

**SDR source (no color signaling):**
```
$ same-blue probe "CosmosLaundromat_2k24p.mp4"

File:              CosmosLaundromat_2k24p.mp4
Resolution:        2048x858
HDR content:       no
HDR metadata:      no
```

**File not found:**
```
$ same-blue probe /nonexistent.mp4

probe failed: exit status 1
Error: Failed to open file: /nonexistent.mp4
```

### Transcode to HDR AV1 (passthrough)

```sh
same-blue transcode -mode hdr input.mp4 output_hdr.mkv
```

Probe the result:
```
File:              output_hdr.mkv
Resolution:        3840x2160
Color Primaries:   BT.2020
Transfer Function: PQ (ST.2084)
Color Space:       BT.2020 NCL
HDR content:       yes
HDR metadata:      no
```

10-bit AV1. BT.2020/PQ color space preserved. Audio is copied without re-encoding.

> Note: `HDR metadata: no` means the SMPTE ST.2086 mastering display block is not yet re-embedded in the output container. The color space signaling itself is correct. Re-embedding side data is a known gap.

### Transcode to SDR AV1 (tone mapped)

```sh
same-blue transcode -mode sdr -peak-nits 1000 input.mp4 output_sdr.mkv
```

Probe the result:
```
File:              output_sdr.mkv
Resolution:        3840x2160
Color Primaries:   BT.709
Transfer Function: BT.709
Color Space:       BT.709
HDR content:       no
HDR metadata:      no
```

8-bit AV1. Full pipeline: PQ decode → Reinhard tone map at 1000 nit → BT.2020→BT.709 matrix → BT.709 gamma.

### High quality encode

```sh
same-blue transcode -mode hdr -preset 2 -crf 20 input.mp4 output_hq.mkv
```

`-preset 2` trades encoding speed for compression efficiency. `-crf 20` targets higher quality than the default 35.

### Auto mode (detect from stream)

```sh
same-blue transcode input.mp4 output.mkv
```

Reads the transfer function from the source stream. PQ or HLG → HDR output. Anything else → SDR output.

---

## Architecture

```
┌─────────────────────────────────────────┐
│         Go CLI  (cmd/same-blue)         │
│  parses flags, formats probe results    │
└──────────┬────────────────┬─────────────┘
           │                │
  internal/runner    internal/probe
  spawns C++ binary  typed CICP constants
  wires flags        JSON → HDRInfo struct
           │
           ▼
┌─────────────────────────────────────────┐
│        C++ binary  (Same_Blue)          │
│                                         │
│  src/hdr/        color spaces, RAII     │
│  src/process/    PQ math, tone mapping  │
│  src/encode/     FFmpeg + SVT-AV1       │
│  src/platform/   macOS Metal / DX11     │
└─────────────────────────────────────────┘
```

Go and C++ communicate through subprocess calls only. The C++ binary writes JSON to **stdout**; diagnostics go to **stderr**. This keeps the interface stable even as C++ internals change.

### Internal JSON interface

`Same_Blue --json <file>` (used by `internal/runner`, not for direct use):

```json
{
  "width": 3840,
  "height": 2160,
  "color_primaries": 9,
  "transfer_function": 16,
  "color_space": 9,
  "is_hdr_content": true,
  "has_hdr_metadata": true,
  "mastering_display": {
    "red_x": 0.68, "red_y": 0.32,
    "green_x": 0.265, "green_y": 0.69,
    "blue_x": 0.15, "blue_y": 0.06,
    "white_x": 0.3127, "white_y": 0.329,
    "min_luminance": 0.01, "max_luminance": 2000.0
  },
  "content_light_level": { "max_cll": 2000, "max_fall": 300 }
}
```

CICP fields are integers (ISO/IEC 23091-2), not strings — display name changes in C++ cannot break Go parsing.

---

## Color Standards

| Standard | Role |
|---|---|
| ITU-R BT.2020 | HDR color primaries and matrix |
| SMPTE ST.2084 (PQ) | HDR transfer function, 0–10,000 nits |
| ARIB STD-B67 (HLG) | Alternative HDR transfer function |
| SMPTE ST.2086 | Mastering display chromaticity and luminance |
| CTA-861.3 | MaxCLL / MaxFALL content light level |
| ISO/IEC 23091-2 (CICP) | Integer coding for all color signaling |
| ITU-R BT.709 | SDR color primaries, matrix, and gamma |

---

## Test Suite

| Suite | Command | Coverage |
|---|---|---|
| C++ color math | `./build/Same_Blue_tests` | 53 checks: PQ round-trip, BT.2020→BT.709 matrix, Reinhard tone mapping, YCbCr decode, full HDR→SDR pipeline |
| Go probe parsing | `go test ./internal/probe/...` | 11 tests: JSON parse, CICP typed constants, null fields, error cases |
| Go runner | `go test ./internal/runner/...` | 11 tests: flag forwarding, `*int` nil/zero distinction, binary path, integration |

---

## Known Gaps

- **SMPTE ST.2086 side data not re-embedded in output:** transcoded files show `HDR metadata: no` even in HDR mode. The color space is correctly signaled; only the mastering display SEI block is missing from the container.
- **No batch processing:** one file per invocation.
- **Windows not yet implemented:** `src/platform/win32_dx11.cpp` is a stub. DXGI Desktop Duplication capture is the next planned phase.

---

## Roadmap

- [x] HDR probe with SMPTE ST.2086 / CTA-861.3 metadata
- [x] HDR AV1 passthrough encode (BT.2020, PQ, 10-bit)
- [x] SDR AV1 encode with Reinhard tone mapping (BT.709, 8-bit)
- [x] Structured JSON interface between C++ and Go
- [x] Typed CICP constants in Go
- [ ] Re-embed SMPTE ST.2086 mastering display in output container
- [ ] Windows DXGI Desktop Duplication real-time capture
- [ ] Windows DirectX 11 rendering backend

---

# Same-Blue（中文）

用于分析 HDR 视频元数据并将视频重新编码为 AV1 的跨平台命令行工具。采用 C++/Go 混合架构：C++ 负责 FFmpeg 解码、色彩数学运算和 SVT-AV1 编码；Go 提供命令行界面和流程编排。

**主要使用场景：** 将 HDR 游戏录像（在 Windows 上通过 DXGI 采集）处理成标准化 AV1 文件——保留 HDR（BT.2020、PQ、10-bit）或转换为 SDR（BT.709、8-bit，Reinhard 色调映射）。

> **平台状态：** macOS 是当前活跃的开发平台。Windows（DXGI 采集）功能计划中。

---

## 环境要求

**macOS：**
```sh
brew install ffmpeg
```

CMake 3.20+、支持 C++20 的编译器（Apple Clang 15+ 或 GCC 13+）、Go 1.22+。

---

## 构建

```sh
cmake -B build -S .
cmake --build build
go build ./cmd/same-blue
```

运行 C++ 单元测试：
```sh
./build/Same_Blue_tests
```

运行 Go 测试：
```sh
go test ./...
```

---

## 使用方法

```
same-blue — HDR video probe and AV1 encoder

用法:
  same-blue probe <文件>
  same-blue transcode [参数] <输入> <输出>

命令:
  probe       打印视频文件的 HDR 元数据
  transcode   将视频重新编码为 AV1

转码参数:
  -mode      string   输出模式: auto|hdr|sdr  (默认 "auto")
  -preset    int      SVT-AV1 预设 0–12       (默认 8)
  -crf       int      质量 0–63               (默认 35)
  -peak-nits float    SDR 色调映射峰值亮度尼特 (默认 1000)
```

---

## 示例

### 探测文件

**HDR10 源文件：**
```
$ same-blue probe "LG Jazz HDR UHD.mp4"

File:              LG Jazz HDR UHD.mp4
Resolution:        3840x2160
Color Primaries:   BT.2020
Transfer Function: PQ (ST.2084)
Color Space:       BT.2020 NCL
HDR content:       yes
HDR metadata:      yes

Mastering Display (SMPTE 2086):
  Red   xy:  (0.68000, 0.32000)
  Green xy:  (0.26496, 0.69000)
  Blue  xy:  (0.15000, 0.06000)
  White xy:  (0.31270, 0.32900)
  Luminance: 0.0100 – 2000.0000 cd/m²

Content Light Level (CTA-861.3):
  MaxCLL:  2000 cd/m²
  MaxFALL: 300 cd/m²
```

**SDR 源文件（无色彩信令）：**
```
$ same-blue probe "CosmosLaundromat_2k24p.mp4"

File:              CosmosLaundromat_2k24p.mp4
Resolution:        2048x858
HDR content:       no
HDR metadata:      no
```

**文件不存在：**
```
$ same-blue probe /nonexistent.mp4

probe failed: exit status 1
Error: Failed to open file: /nonexistent.mp4
```

### 转码为 HDR AV1（直通模式）

```sh
same-blue transcode -mode hdr input.mp4 output_hdr.mkv
```

探测结果：
```
File:              output_hdr.mkv
Resolution:        3840x2160
Color Primaries:   BT.2020
Transfer Function: PQ (ST.2084)
Color Space:       BT.2020 NCL
HDR content:       yes
HDR metadata:      no
```

10-bit AV1，BT.2020/PQ 色彩空间完整保留。音频直接复制，不重新编码。

> 注意：`HDR metadata: no` 表示 SMPTE ST.2086 母版显示数据块尚未被重新嵌入输出容器。色彩空间信令本身是正确的。重新嵌入侧数据是已知的待完善功能。

### 转码为 SDR AV1（色调映射）

```sh
same-blue transcode -mode sdr -peak-nits 1000 input.mp4 output_sdr.mkv
```

探测结果：
```
File:              output_sdr.mkv
Resolution:        3840x2160
Color Primaries:   BT.709
Transfer Function: BT.709
Color Space:       BT.709
HDR content:       no
HDR metadata:      no
```

8-bit AV1。完整流水线：PQ 解码 → 1000 尼特 Reinhard 色调映射 → BT.2020→BT.709 矩阵转换 → BT.709 伽马编码。

### 高质量编码

```sh
same-blue transcode -mode hdr -preset 2 -crf 20 input.mp4 output_hq.mkv
```

`-preset 2` 以编码速度换取压缩效率。`-crf 20` 目标质量高于默认值 35。

### 自动模式（从码流检测）

```sh
same-blue transcode input.mp4 output.mkv
```

从源码流读取传递函数。PQ 或 HLG → HDR 输出；其他 → SDR 输出。

---

## 架构

```
┌─────────────────────────────────────────┐
│      Go CLI 层  (cmd/same-blue)         │
│  解析参数、格式化探测结果                │
└──────────┬────────────────┬─────────────┘
           │                │
  internal/runner    internal/probe
  启动 C++ 子进程     类型化 CICP 常量
  传递命令行参数      JSON → HDRInfo 结构体
           │
           ▼
┌─────────────────────────────────────────┐
│     C++ 二进制文件  (Same_Blue)         │
│                                         │
│  src/hdr/        色彩空间、RAII 封装    │
│  src/process/    PQ 数学、色调映射      │
│  src/encode/     FFmpeg + SVT-AV1       │
│  src/platform/   macOS Metal / DX11     │
└─────────────────────────────────────────┘
```

Go 和 C++ 仅通过子进程调用进行通信。C++ 二进制文件将 JSON 写入 **stdout**；诊断信息写入 **stderr**。这样即使 C++ 内部实现发生变化，接口也保持稳定。

### 内部 JSON 接口

`Same_Blue --json <文件>`（由 `internal/runner` 使用，不供直接调用）：

```json
{
  "width": 3840,
  "height": 2160,
  "color_primaries": 9,
  "transfer_function": 16,
  "color_space": 9,
  "is_hdr_content": true,
  "has_hdr_metadata": true,
  "mastering_display": {
    "red_x": 0.68, "red_y": 0.32,
    "green_x": 0.265, "green_y": 0.69,
    "blue_x": 0.15, "blue_y": 0.06,
    "white_x": 0.3127, "white_y": 0.329,
    "min_luminance": 0.01, "max_luminance": 2000.0
  },
  "content_light_level": { "max_cll": 2000, "max_fall": 300 }
}
```

CICP 字段使用整数（ISO/IEC 23091-2），而非字符串——C++ 端显示名称的变更不会破坏 Go 端的解析。

---

## 色彩标准

| 标准 | 作用 |
|---|---|
| ITU-R BT.2020 | HDR 色彩原色与矩阵 |
| SMPTE ST.2084（PQ） | HDR 传递函数，0–10,000 尼特 |
| ARIB STD-B67（HLG） | 替代 HDR 传递函数 |
| SMPTE ST.2086 | 母版显示色度坐标与亮度 |
| CTA-861.3 | MaxCLL / MaxFALL 内容亮度级别 |
| ISO/IEC 23091-2（CICP） | 所有色彩信令的整数编码 |
| ITU-R BT.709 | SDR 色彩原色、矩阵与伽马 |

---

## 测试套件

| 套件 | 命令 | 覆盖范围 |
|---|---|---|
| C++ 色彩数学 | `./build/Same_Blue_tests` | 53 项检查：PQ 往返、BT.2020→BT.709 矩阵、Reinhard 色调映射、YCbCr 解码、完整 HDR→SDR 流水线 |
| Go 探测解析 | `go test ./internal/probe/...` | 11 项测试：JSON 解析、CICP 类型化常量、null 字段、错误处理 |
| Go 运行器 | `go test ./internal/runner/...` | 11 项测试：参数传递、`*int` 空值/零值区分、二进制路径、集成测试 |

---

## 已知问题

- **SMPTE ST.2086 侧数据未重新嵌入输出：** 转码文件即使在 HDR 模式下也显示 `HDR metadata: no`。色彩空间信令是正确的，但容器中缺少母版显示 SEI 数据块。
- **不支持批量处理：** 每次调用只处理一个文件。
- **Windows 尚未实现：** `src/platform/win32_dx11.cpp` 为占位文件。DXGI Desktop Duplication 采集是下一个计划阶段。

---

## 路线图

- [x] HDR 探测，支持 SMPTE ST.2086 / CTA-861.3 元数据
- [x] HDR AV1 直通编码（BT.2020、PQ、10-bit）
- [x] SDR AV1 编码，含 Reinhard 色调映射（BT.709、8-bit）
- [x] C++ 与 Go 之间的结构化 JSON 接口
- [x] Go 中的类型化 CICP 常量
- [ ] 将 SMPTE ST.2086 母版显示数据重新嵌入输出容器
- [ ] Windows DXGI Desktop Duplication 实时采集
- [ ] Windows DirectX 11 渲染后端
