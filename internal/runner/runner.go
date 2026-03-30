package runner

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"strconv"
)

// Runner locates and invokes the Same_Blue C++ binary.
type Runner struct {
	// BinaryPath is the path to the Same_Blue executable.
	// Defaults to "./build/Same_Blue" if empty.
	BinaryPath string
}

// TranscodeConfig mirrors encode::Config in pipeline.h.
// Preset and CRF use pointers so that zero is a valid value (preset 0 = max quality).
// A nil pointer means "omit the flag and let the C++ binary use its default".
type TranscodeConfig struct {
	Mode     string  // "auto" | "hdr" | "sdr"
	Preset   *int    // SVT-AV1 preset 0–12; nil = C++ default (8)
	CRF      *int    // quality 0–63; nil = C++ default (35)
	PeakNits float64 // mastering peak nits for SDR tone mapping; 0 = C++ default (1000)
}

// Probe runs `Same_Blue --json <filepath>` and returns the JSON written to stdout.
// Stderr is kept separate so that diagnostic messages don't corrupt the JSON.
func (r *Runner) Probe(filepath string) (string, error) {
	var stdout, stderr bytes.Buffer
	cmd := exec.Command(r.binary(), "--json", filepath)
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Run(); err != nil {
		return "", fmt.Errorf("probe failed: %w\n%s", err, stderr.String())
	}
	return stdout.String(), nil
}

// Transcode runs `Same_Blue --transcode <input> <output>` and streams
// progress lines to stdout in real time.
func (r *Runner) Transcode(input, output string, cfg TranscodeConfig) error {
	cmd := exec.Command(r.binary(), r.transcodeArgs(input, output, cfg)...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("transcode failed: %w", err)
	}
	return nil
}

func (r *Runner) transcodeArgs(input, output string, cfg TranscodeConfig) []string {
	args := []string{"--transcode", input, output}
	if cfg.Mode != "" {
		args = append(args, "--mode", cfg.Mode)
	}
	if cfg.Preset != nil {
		args = append(args, "--preset", strconv.Itoa(*cfg.Preset))
	}
	if cfg.CRF != nil {
		args = append(args, "--crf", strconv.Itoa(*cfg.CRF))
	}
	if cfg.PeakNits != 0 {
		args = append(args, "--peak-nits", strconv.FormatFloat(cfg.PeakNits, 'f', -1, 64))
	}
	return args
}

func (r *Runner) binary() string {
	if r.BinaryPath != "" {
		return r.BinaryPath
	}
	return "./build/Same_Blue"
}

// IntPtr is a convenience helper for constructing *int values inline,
// useful when calling TranscodeConfig with literal preset/CRF values.
func IntPtr(v int) *int { return &v }
