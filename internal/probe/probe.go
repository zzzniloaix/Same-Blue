package probe

import (
	"encoding/json"
	"fmt"
)

// ---------------------------------------------------------------------------
// CICP typed constants (ISO/IEC 23091-2)
// ---------------------------------------------------------------------------

// ColorPrimaries mirrors hdr::ColorPrimaries in color_spaces.h.
type ColorPrimaries uint8

const (
	ColorPrimariesBT709    ColorPrimaries = 1
	ColorPrimariesBT470M   ColorPrimaries = 4
	ColorPrimariesBT470BG  ColorPrimaries = 5
	ColorPrimaries170M     ColorPrimaries = 6
	ColorPrimaries240M     ColorPrimaries = 7
	ColorPrimariesFilm     ColorPrimaries = 8
	ColorPrimariesBT2020   ColorPrimaries = 9
	ColorPrimariesSMPTE428 ColorPrimaries = 10
	ColorPrimariesSMPTE431 ColorPrimaries = 11
	ColorPrimariesSMPTE432 ColorPrimaries = 12
	ColorPrimariesEBU3213  ColorPrimaries = 22
)

func (p ColorPrimaries) String() string {
	switch p {
	case ColorPrimariesBT709:
		return "BT.709"
	case ColorPrimariesBT470M:
		return "BT.470M"
	case ColorPrimariesBT470BG:
		return "BT.470BG"
	case ColorPrimaries170M:
		return "SMPTE 170M"
	case ColorPrimaries240M:
		return "SMPTE 240M"
	case ColorPrimariesFilm:
		return "Generic Film"
	case ColorPrimariesBT2020:
		return "BT.2020"
	case ColorPrimariesSMPTE428:
		return "SMPTE 428"
	case ColorPrimariesSMPTE431:
		return "DCI-P3"
	case ColorPrimariesSMPTE432:
		return "Display P3"
	case ColorPrimariesEBU3213:
		return "EBU 3213"
	default:
		return fmt.Sprintf("Unknown(%d)", uint8(p))
	}
}

// TransferCharacteristics mirrors hdr::TransferFunction in color_spaces.h.
type TransferCharacteristics uint8

const (
	TransferBT709       TransferCharacteristics = 1
	TransferGamma22     TransferCharacteristics = 4
	TransferGamma28     TransferCharacteristics = 5
	Transfer170M        TransferCharacteristics = 6
	Transfer240M        TransferCharacteristics = 7
	TransferLinear      TransferCharacteristics = 8
	TransferLog100      TransferCharacteristics = 9
	TransferLogSqrt     TransferCharacteristics = 10
	TransferIEC61966_24 TransferCharacteristics = 11
	TransferBT1361ECG   TransferCharacteristics = 12
	TransferSRGB        TransferCharacteristics = 13
	TransferBT2020_10   TransferCharacteristics = 14
	TransferBT2020_12   TransferCharacteristics = 15
	TransferPQ          TransferCharacteristics = 16
	TransferSMPTE428    TransferCharacteristics = 17
	TransferHLG         TransferCharacteristics = 18
)

func (t TransferCharacteristics) String() string {
	switch t {
	case TransferBT709:
		return "BT.709"
	case TransferGamma22:
		return "Gamma 2.2"
	case TransferGamma28:
		return "Gamma 2.8"
	case Transfer170M:
		return "SMPTE 170M"
	case Transfer240M:
		return "SMPTE 240M"
	case TransferLinear:
		return "Linear"
	case TransferLog100:
		return "Log 100:1"
	case TransferLogSqrt:
		return "Log 316:1"
	case TransferIEC61966_24:
		return "IEC 61966-2-4"
	case TransferBT1361ECG:
		return "BT.1361 ECG"
	case TransferSRGB:
		return "sRGB"
	case TransferBT2020_10:
		return "BT.2020 10-bit"
	case TransferBT2020_12:
		return "BT.2020 12-bit"
	case TransferPQ:
		return "PQ (ST.2084)"
	case TransferSMPTE428:
		return "SMPTE 428"
	case TransferHLG:
		return "HLG"
	default:
		return fmt.Sprintf("Unknown(%d)", uint8(t))
	}
}

// MatrixCoefficients mirrors hdr::ColorSpace in color_spaces.h.
type MatrixCoefficients uint8

const (
	MatrixGBR            MatrixCoefficients = 0
	MatrixBT709          MatrixCoefficients = 1
	MatrixFCC            MatrixCoefficients = 4
	MatrixBT470BG        MatrixCoefficients = 5
	Matrix170M           MatrixCoefficients = 6
	Matrix240M           MatrixCoefficients = 7
	MatrixYCgCo          MatrixCoefficients = 8
	MatrixBT2020NCL      MatrixCoefficients = 9
	MatrixBT2020CL       MatrixCoefficients = 10
	MatrixSMPTE2085      MatrixCoefficients = 11
	MatrixChromaDNCL     MatrixCoefficients = 12
	MatrixChromaDCL      MatrixCoefficients = 13
	MatrixICTCP          MatrixCoefficients = 14
)

func (m MatrixCoefficients) String() string {
	switch m {
	case MatrixGBR:
		return "GBR"
	case MatrixBT709:
		return "BT.709"
	case MatrixFCC:
		return "FCC"
	case MatrixBT470BG:
		return "BT.470BG"
	case Matrix170M:
		return "SMPTE 170M"
	case Matrix240M:
		return "SMPTE 240M"
	case MatrixYCgCo:
		return "YCgCo"
	case MatrixBT2020NCL:
		return "BT.2020 NCL"
	case MatrixBT2020CL:
		return "BT.2020 CL"
	case MatrixSMPTE2085:
		return "SMPTE 2085"
	case MatrixChromaDNCL:
		return "Chroma-derived NCL"
	case MatrixChromaDCL:
		return "Chroma-derived CL"
	case MatrixICTCP:
		return "ICtCp"
	default:
		return fmt.Sprintf("Unknown(%d)", uint8(m))
	}
}

// ---------------------------------------------------------------------------
// HDR data types
// ---------------------------------------------------------------------------

// MasteringDisplay holds SMPTE ST 2086 mastering display metadata.
type MasteringDisplay struct {
	RedX   float64 `json:"red_x"`
	RedY   float64 `json:"red_y"`
	GreenX float64 `json:"green_x"`
	GreenY float64 `json:"green_y"`
	BlueX  float64 `json:"blue_x"`
	BlueY  float64 `json:"blue_y"`
	WhiteX float64 `json:"white_x"`
	WhiteY float64 `json:"white_y"`
	MinLuminance float64 `json:"min_luminance"`
	MaxLuminance float64 `json:"max_luminance"`
}

// ContentLightLevel holds CTA-861.3 content light level metadata.
type ContentLightLevel struct {
	MaxCLL  int `json:"max_cll"`
	MaxFALL int `json:"max_fall"`
}

// HDRInfo holds the parsed output of `Same_Blue --json <file>`.
type HDRInfo struct {
	Width  int `json:"width"`
	Height int `json:"height"`

	// CICP typed pointers; nil when the stream does not signal a value.
	ColorPrimaries          *ColorPrimaries          `json:"color_primaries"`
	TransferCharacteristics *TransferCharacteristics `json:"transfer_function"`
	MatrixCoefficients      *MatrixCoefficients      `json:"color_space"`

	IsHDRContent   bool `json:"is_hdr_content"`
	HasHDRMetadata bool `json:"has_hdr_metadata"`

	MasteringDisplay  *MasteringDisplay  `json:"mastering_display"`
	ContentLightLevel *ContentLightLevel `json:"content_light_level"`
}

// ---------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------

// Parse parses the JSON produced by `Same_Blue --json <file>`.
func Parse(output string) (*HDRInfo, error) {
	if output == "" {
		return nil, fmt.Errorf("no probe output")
	}
	var info HDRInfo
	if err := json.Unmarshal([]byte(output), &info); err != nil {
		return nil, fmt.Errorf("failed to parse probe JSON: %w", err)
	}
	if info.Width == 0 && info.Height == 0 {
		return nil, fmt.Errorf("no video stream information found in probe output")
	}
	return &info, nil
}
