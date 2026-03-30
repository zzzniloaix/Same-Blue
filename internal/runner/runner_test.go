package runner

import (
	"os"
	"reflect"
	"testing"
)

// ---------------------------------------------------------------------------
// binary() path resolution
// ---------------------------------------------------------------------------

func TestBinaryDefault(t *testing.T) {
	r := &Runner{}
	if got := r.binary(); got != "./build/Same_Blue" {
		t.Errorf("binary(): got %q, want %q", got, "./build/Same_Blue")
	}
}

func TestBinaryCustomPath(t *testing.T) {
	r := &Runner{BinaryPath: "/usr/local/bin/Same_Blue"}
	if got := r.binary(); got != "/usr/local/bin/Same_Blue" {
		t.Errorf("binary(): got %q, want %q", got, "/usr/local/bin/Same_Blue")
	}
}

// ---------------------------------------------------------------------------
// transcodeArgs — flag forwarding
// ---------------------------------------------------------------------------

func TestTranscodeArgs_Defaults(t *testing.T) {
	r := &Runner{}
	args := r.transcodeArgs("in.mp4", "out.mkv", TranscodeConfig{})
	want := []string{"--transcode", "in.mp4", "out.mkv"}
	if !reflect.DeepEqual(args, want) {
		t.Errorf("got %v, want %v", args, want)
	}
}

func TestTranscodeArgs_AllFlags(t *testing.T) {
	r := &Runner{}
	cfg := TranscodeConfig{Mode: "sdr", Preset: IntPtr(6), CRF: IntPtr(28), PeakNits: 2000.0}
	args := r.transcodeArgs("in.mp4", "out.mkv", cfg)
	want := []string{
		"--transcode", "in.mp4", "out.mkv",
		"--mode", "sdr",
		"--preset", "6",
		"--crf", "28",
		"--peak-nits", "2000",
	}
	if !reflect.DeepEqual(args, want) {
		t.Errorf("got %v, want %v", args, want)
	}
}

func TestTranscodeArgs_ModeOnly(t *testing.T) {
	r := &Runner{}
	args := r.transcodeArgs("a.mp4", "b.mkv", TranscodeConfig{Mode: "hdr"})
	want := []string{"--transcode", "a.mp4", "b.mkv", "--mode", "hdr"}
	if !reflect.DeepEqual(args, want) {
		t.Errorf("got %v, want %v", args, want)
	}
}

func TestTranscodeArgs_NilValuesOmitted(t *testing.T) {
	// nil Preset and CRF must be omitted — zero is now a valid value
	r := &Runner{}
	args := r.transcodeArgs("a.mp4", "b.mkv", TranscodeConfig{})
	want := []string{"--transcode", "a.mp4", "b.mkv"}
	if !reflect.DeepEqual(args, want) {
		t.Errorf("got %v, want %v", args, want)
	}
}

func TestTranscodeArgs_PresetZeroIsValid(t *testing.T) {
	// Preset=0 is the max-quality SVT-AV1 setting and must be forwarded, not omitted
	r := &Runner{}
	args := r.transcodeArgs("a.mp4", "b.mkv", TranscodeConfig{Preset: IntPtr(0)})
	want := []string{"--transcode", "a.mp4", "b.mkv", "--preset", "0"}
	if !reflect.DeepEqual(args, want) {
		t.Errorf("got %v, want %v", args, want)
	}
}

func TestTranscodeArgs_CRFZeroIsValid(t *testing.T) {
	r := &Runner{}
	args := r.transcodeArgs("a.mp4", "b.mkv", TranscodeConfig{CRF: IntPtr(0)})
	want := []string{"--transcode", "a.mp4", "b.mkv", "--crf", "0"}
	if !reflect.DeepEqual(args, want) {
		t.Errorf("got %v, want %v", args, want)
	}
}

func TestTranscodeArgs_PeakNitsFloat(t *testing.T) {
	r := &Runner{}
	args := r.transcodeArgs("a.mp4", "b.mkv", TranscodeConfig{PeakNits: 1000.5})
	found := false
	for i, a := range args {
		if a == "--peak-nits" && i+1 < len(args) {
			if args[i+1] != "1000.5" {
				t.Errorf("--peak-nits value: got %q, want %q", args[i+1], "1000.5")
			}
			found = true
		}
	}
	if !found {
		t.Errorf("--peak-nits flag not found in args: %v", args)
	}
}

// ---------------------------------------------------------------------------
// Probe — error path (no binary needed)
// ---------------------------------------------------------------------------

func TestProbe_BinaryNotFound(t *testing.T) {
	r := &Runner{BinaryPath: "/nonexistent/Same_Blue"}
	_, err := r.Probe("any.mp4")
	if err == nil {
		t.Error("expected error for missing binary, got nil")
	}
}

// ---------------------------------------------------------------------------
// Integration — skipped unless the built binary is present
// ---------------------------------------------------------------------------

func TestProbe_Integration(t *testing.T) {
	const binary = "./../../build/Same_Blue"
	if _, err := os.Stat(binary); os.IsNotExist(err) {
		t.Skip("binary not found, skipping integration test")
	}
	r := &Runner{BinaryPath: binary}
	_, err := r.Probe("/nonexistent_file.mp4")
	if err == nil {
		t.Error("expected error probing nonexistent file, got nil")
	}
}
