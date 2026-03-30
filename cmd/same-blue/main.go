package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/jiayu-lin/same-blue/internal/probe"
	"github.com/jiayu-lin/same-blue/internal/runner"
)

const usage = `same-blue — HDR video probe and AV1 encoder

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
  -peak-nits float    SDR tone mapping peak nits (default 1000, C++ side)
`

func main() {
	flag.Usage = func() { fmt.Fprint(os.Stderr, usage) }
	flag.Parse()

	if flag.NArg() < 1 {
		flag.Usage()
		os.Exit(1)
	}

	r := &runner.Runner{}

	switch flag.Arg(0) {
	case "probe":
		if flag.NArg() < 2 {
			fmt.Fprintln(os.Stderr, "probe: missing <file> argument")
			os.Exit(1)
		}
		runProbe(r, flag.Arg(1))

	case "transcode":
		// flag.Args() is the post-parse remainder; [0] is "transcode", [1:] is its args.
		runTranscode(r, flag.Args()[1:])

	default:
		fmt.Fprintf(os.Stderr, "unknown command: %s\n\n", flag.Arg(0))
		flag.Usage()
		os.Exit(1)
	}
}

func runProbe(r *runner.Runner, filepath string) {
	raw, err := r.Probe(filepath)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	info, err := probe.Parse(raw)
	if err != nil {
		fmt.Fprintln(os.Stderr, "failed to parse probe output:", err)
		os.Exit(1)
	}

	fmt.Printf("File:              %s\n", filepath)
	fmt.Printf("Resolution:        %dx%d\n", info.Width, info.Height)
	if info.ColorPrimaries != nil {
		fmt.Printf("Color Primaries:   %s\n", info.ColorPrimaries)
	}
	if info.TransferCharacteristics != nil {
		fmt.Printf("Transfer Function: %s\n", info.TransferCharacteristics)
	}
	if info.MatrixCoefficients != nil {
		fmt.Printf("Color Space:       %s\n", info.MatrixCoefficients)
	}
	fmt.Printf("HDR content:       %v\n", yesNo(info.IsHDRContent))
	fmt.Printf("HDR metadata:      %v\n", yesNo(info.HasHDRMetadata))

	if md := info.MasteringDisplay; md != nil {
		fmt.Println("\nMastering Display (SMPTE 2086):")
		fmt.Printf("  Red   xy:  (%.5f, %.5f)\n", md.RedX, md.RedY)
		fmt.Printf("  Green xy:  (%.5f, %.5f)\n", md.GreenX, md.GreenY)
		fmt.Printf("  Blue  xy:  (%.5f, %.5f)\n", md.BlueX, md.BlueY)
		fmt.Printf("  White xy:  (%.5f, %.5f)\n", md.WhiteX, md.WhiteY)
		fmt.Printf("  Luminance: %.4f – %.4f cd/m²\n", md.MinLuminance, md.MaxLuminance)
	}

	if cll := info.ContentLightLevel; cll != nil {
		fmt.Println("\nContent Light Level (CTA-861.3):")
		fmt.Printf("  MaxCLL:  %d cd/m²\n", cll.MaxCLL)
		fmt.Printf("  MaxFALL: %d cd/m²\n", cll.MaxFALL)
	}
}

func runTranscode(r *runner.Runner, args []string) {
	fs       := flag.NewFlagSet("transcode", flag.ExitOnError)
	mode     := fs.String("mode",      "auto", "Output mode: auto|hdr|sdr")
	preset   := fs.Int("preset",       8,      "SVT-AV1 preset 0–12")
	crf      := fs.Int("crf",          35,     "Quality 0–63")
	peakNits := fs.Float64("peak-nits", 0,     "Mastering display peak in nits for SDR tone mapping (default: 1000)")

	if err := fs.Parse(args); err != nil {
		os.Exit(1)
	}
	if fs.NArg() < 2 {
		fmt.Fprintln(os.Stderr, "transcode: missing <input> and/or <output> arguments")
		os.Exit(1)
	}

	cfg := runner.TranscodeConfig{
		Mode:     *mode,
		Preset:   preset,   // *int — zero is valid (max quality preset)
		CRF:      crf,      // *int — zero is valid
		PeakNits: *peakNits,
	}

	if err := r.Transcode(fs.Arg(0), fs.Arg(1), cfg); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func yesNo(b bool) string {
	if b {
		return "yes"
	}
	return "no"
}
