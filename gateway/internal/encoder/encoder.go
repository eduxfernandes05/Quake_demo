// Package encoder wraps FFmpeg and libopus for media encoding.
//
// Video: RGBA frames → H.264 NAL units
// Audio: PCM samples → Opus packets
package encoder

import (
	"log/slog"
)

// VideoEncoder encodes raw RGBA frames to H.264 NAL units.
type VideoEncoder struct {
	Width   int
	Height  int
	Bitrate int // bits per second
}

// NewVideoEncoder creates a video encoder with the given parameters.
// In production this wraps FFmpeg's libavcodec via CGo.
func NewVideoEncoder(width, height, bitrate int) *VideoEncoder {
	slog.Info("video encoder initialized", "width", width, "height", height, "bitrate", bitrate)
	return &VideoEncoder{Width: width, Height: height, Bitrate: bitrate}
}

// Encode converts an RGBA frame to H.264 NAL units.
// Returns the encoded bytes or an error.
func (e *VideoEncoder) Encode(rgba []byte) ([]byte, error) {
	// Placeholder — real implementation calls x264/FFmpeg via CGo
	return nil, nil
}

// Close releases encoder resources.
func (e *VideoEncoder) Close() {
	slog.Info("video encoder closed")
}

// AudioEncoder encodes raw PCM samples to Opus packets.
type AudioEncoder struct {
	SampleRate int
	Channels   int
	Bitrate    int
}

// NewAudioEncoder creates an Opus audio encoder.
// In production this wraps libopus via CGo.
func NewAudioEncoder(sampleRate, channels, bitrate int) *AudioEncoder {
	slog.Info("audio encoder initialized", "rate", sampleRate, "channels", channels, "bitrate", bitrate)
	return &AudioEncoder{SampleRate: sampleRate, Channels: channels, Bitrate: bitrate}
}

// Encode converts PCM samples to an Opus packet.
func (e *AudioEncoder) Encode(pcm []int16) ([]byte, error) {
	// Placeholder — real implementation calls libopus via CGo
	return nil, nil
}

// Close releases encoder resources.
func (e *AudioEncoder) Close() {
	slog.Info("audio encoder closed")
}
