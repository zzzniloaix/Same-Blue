package probe

import (
	"math"
	"testing"
)

// fullHDRJSON is representative JSON produced by `Same_Blue --json` for a real HDR10 file.
const fullHDRJSON = `{
  "width": 3840,
  "height": 2160,
  "color_primaries": 9,
  "transfer_function": 16,
  "color_space": 9,
  "is_hdr_content": true,
  "has_hdr_metadata": true,
  "mastering_display": {
    "red_x": 0.68000,
    "red_y": 0.32000,
    "green_x": 0.26496,
    "green_y": 0.69000,
    "blue_x": 0.15000,
    "blue_y": 0.06000,
    "white_x": 0.31270,
    "white_y": 0.32900,
    "min_luminance": 0.0100,
    "max_luminance": 2000.0000
  },
  "content_light_level": {
    "max_cll": 2000,
    "max_fall": 300
  }
}`

const sdrJSON = `{
  "width": 1920,
  "height": 1080,
  "color_primaries": 1,
  "transfer_function": 1,
  "color_space": 1,
  "is_hdr_content": false,
  "has_hdr_metadata": false
}`

const noMetadataHDRJSON = `{
  "width": 1920,
  "height": 1080,
  "color_primaries": 9,
  "transfer_function": 16,
  "color_space": 9,
  "is_hdr_content": true,
  "has_hdr_metadata": false
}`

const nullCICPJSON = `{
  "width": 640,
  "height": 480,
  "color_primaries": null,
  "transfer_function": null,
  "color_space": null,
  "is_hdr_content": false,
  "has_hdr_metadata": false
}`

func TestParse_FullHDR10(t *testing.T) {
	info, err := Parse(fullHDRJSON)
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}

	if info.Width != 3840 || info.Height != 2160 {
		t.Errorf("resolution: got %dx%d, want 3840x2160", info.Width, info.Height)
	}

	if info.ColorPrimaries == nil {
		t.Fatal("ColorPrimaries is nil")
	}
	if *info.ColorPrimaries != ColorPrimariesBT2020 {
		t.Errorf("color primaries: got %d, want %d (BT.2020)", *info.ColorPrimaries, ColorPrimariesBT2020)
	}
	if info.ColorPrimaries.String() != "BT.2020" {
		t.Errorf("ColorPrimaries.String(): got %q, want %q", info.ColorPrimaries.String(), "BT.2020")
	}

	if info.TransferCharacteristics == nil {
		t.Fatal("TransferCharacteristics is nil")
	}
	if *info.TransferCharacteristics != TransferPQ {
		t.Errorf("transfer: got %d, want %d (PQ)", *info.TransferCharacteristics, TransferPQ)
	}
	if info.TransferCharacteristics.String() != "PQ (ST.2084)" {
		t.Errorf("TransferCharacteristics.String(): got %q, want %q", info.TransferCharacteristics.String(), "PQ (ST.2084)")
	}

	if info.MatrixCoefficients == nil {
		t.Fatal("MatrixCoefficients is nil")
	}
	if *info.MatrixCoefficients != MatrixBT2020NCL {
		t.Errorf("matrix: got %d, want %d (BT.2020 NCL)", *info.MatrixCoefficients, MatrixBT2020NCL)
	}
	if info.MatrixCoefficients.String() != "BT.2020 NCL" {
		t.Errorf("MatrixCoefficients.String(): got %q, want %q", info.MatrixCoefficients.String(), "BT.2020 NCL")
	}

	if !info.IsHDRContent {
		t.Error("IsHDRContent: got false, want true")
	}
	if !info.HasHDRMetadata {
		t.Error("HasHDRMetadata: got false, want true")
	}
}

func TestParse_MasteringDisplay(t *testing.T) {
	info, err := Parse(fullHDRJSON)
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}

	md := info.MasteringDisplay
	if md == nil {
		t.Fatal("MasteringDisplay is nil")
	}

	cases := []struct {
		name string
		got  float64
		want float64
	}{
		{"RedX", md.RedX, 0.68000},
		{"RedY", md.RedY, 0.32000},
		{"GreenX", md.GreenX, 0.26496},
		{"GreenY", md.GreenY, 0.69000},
		{"BlueX", md.BlueX, 0.15000},
		{"BlueY", md.BlueY, 0.06000},
		{"WhiteX", md.WhiteX, 0.31270},
		{"WhiteY", md.WhiteY, 0.32900},
		{"MinLuminance", md.MinLuminance, 0.0100},
		{"MaxLuminance", md.MaxLuminance, 2000.0},
	}
	for _, c := range cases {
		if math.Abs(c.got-c.want) > 1e-5 {
			t.Errorf("MasteringDisplay.%s: got %f, want %f", c.name, c.got, c.want)
		}
	}
}

func TestParse_ContentLightLevel(t *testing.T) {
	info, err := Parse(fullHDRJSON)
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}

	cll := info.ContentLightLevel
	if cll == nil {
		t.Fatal("ContentLightLevel is nil")
	}
	if cll.MaxCLL != 2000 {
		t.Errorf("MaxCLL: got %d, want 2000", cll.MaxCLL)
	}
	if cll.MaxFALL != 300 {
		t.Errorf("MaxFALL: got %d, want 300", cll.MaxFALL)
	}
}

func TestParse_SDR(t *testing.T) {
	info, err := Parse(sdrJSON)
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}

	if info.Width != 1920 || info.Height != 1080 {
		t.Errorf("resolution: got %dx%d, want 1920x1080", info.Width, info.Height)
	}
	if info.IsHDRContent {
		t.Error("IsHDRContent: got true, want false")
	}
	if info.HasHDRMetadata {
		t.Error("HasHDRMetadata: got true, want false")
	}
	if info.MasteringDisplay != nil {
		t.Error("MasteringDisplay: expected nil for SDR file")
	}
	if info.ContentLightLevel != nil {
		t.Error("ContentLightLevel: expected nil for SDR file")
	}
}

func TestParse_HDRContentWithoutMetadata(t *testing.T) {
	info, err := Parse(noMetadataHDRJSON)
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}

	if !info.IsHDRContent {
		t.Error("IsHDRContent: got false, want true")
	}
	if info.HasHDRMetadata {
		t.Error("HasHDRMetadata: got true, want false")
	}
	if info.MasteringDisplay != nil {
		t.Error("MasteringDisplay: expected nil when no metadata section present")
	}
}

func TestParse_NullCICPFields(t *testing.T) {
	info, err := Parse(nullCICPJSON)
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}
	if info.ColorPrimaries != nil {
		t.Errorf("ColorPrimaries: expected nil for null JSON, got %d", *info.ColorPrimaries)
	}
	if info.TransferCharacteristics != nil {
		t.Errorf("TransferCharacteristics: expected nil for null JSON, got %d", *info.TransferCharacteristics)
	}
	if info.MatrixCoefficients != nil {
		t.Errorf("MatrixCoefficients: expected nil for null JSON, got %d", *info.MatrixCoefficients)
	}
}

func TestParse_Empty(t *testing.T) {
	_, err := Parse("")
	if err == nil {
		t.Error("expected error for empty output, got nil")
	}
}

func TestParse_InvalidJSON(t *testing.T) {
	_, err := Parse("not json")
	if err == nil {
		t.Error("expected error for invalid JSON, got nil")
	}
}

func TestColorPrimaries_String(t *testing.T) {
	cases := []struct {
		p    ColorPrimaries
		want string
	}{
		{ColorPrimariesBT709, "BT.709"},
		{ColorPrimariesBT2020, "BT.2020"},
		{ColorPrimariesSMPTE431, "DCI-P3"},
		{ColorPrimariesSMPTE432, "Display P3"},
		{ColorPrimaries(99), "Unknown(99)"},
	}
	for _, c := range cases {
		if got := c.p.String(); got != c.want {
			t.Errorf("ColorPrimaries(%d).String() = %q, want %q", uint8(c.p), got, c.want)
		}
	}
}

func TestTransferCharacteristics_String(t *testing.T) {
	cases := []struct {
		t    TransferCharacteristics
		want string
	}{
		{TransferBT709, "BT.709"},
		{TransferPQ, "PQ (ST.2084)"},
		{TransferHLG, "HLG"},
		{TransferSRGB, "sRGB"},
		{TransferCharacteristics(99), "Unknown(99)"},
	}
	for _, c := range cases {
		if got := c.t.String(); got != c.want {
			t.Errorf("TransferCharacteristics(%d).String() = %q, want %q", uint8(c.t), got, c.want)
		}
	}
}

func TestMatrixCoefficients_String(t *testing.T) {
	cases := []struct {
		m    MatrixCoefficients
		want string
	}{
		{MatrixBT709, "BT.709"},
		{MatrixBT2020NCL, "BT.2020 NCL"},
		{MatrixBT2020CL, "BT.2020 CL"},
		{MatrixCoefficients(99), "Unknown(99)"},
	}
	for _, c := range cases {
		if got := c.m.String(); got != c.want {
			t.Errorf("MatrixCoefficients(%d).String() = %q, want %q", uint8(c.m), got, c.want)
		}
	}
}
