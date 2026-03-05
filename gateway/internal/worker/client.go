// Package worker connects the gateway to the headless Quake worker.
//
// Communication is via a local socket or shared memory. The client
// retrieves captured RGBA frames and PCM audio from the worker and
// forwards injected input events.
package worker

import (
	"fmt"
	"log/slog"
	"sync"
)

// FrameData holds a single captured video frame.
type FrameData struct {
	RGBA   []byte
	Width  int
	Height int
}

// AudioData holds a chunk of captured PCM audio.
type AudioData struct {
	PCM      []int16
	Samples  int
	Rate     int
	Channels int
}

// InputEvent represents a keyboard or mouse event sent to the worker.
type InputEvent struct {
	Type    string `json:"type"`    // "key" or "mouse"
	Key     int    `json:"key"`     // scancode (for key events)
	Down    bool   `json:"down"`    // press/release (for key events)
	DX      int    `json:"dx"`      // mouse delta X
	DY      int    `json:"dy"`      // mouse delta Y
	Buttons int    `json:"buttons"` // mouse button bitmask
}

// Client manages the connection to a single game worker.
type Client struct {
	addr   string
	mu     sync.Mutex
	closed bool
}

// NewClient creates a worker client pointing at the given address.
func NewClient(addr string) *Client {
	return &Client{addr: addr}
}

// CaptureFrame retrieves the latest RGBA frame from the worker.
// In production this would use shared memory or a socket; here we
// provide the interface for upstream consumers.
func (c *Client) CaptureFrame() (*FrameData, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed {
		return nil, fmt.Errorf("worker client closed")
	}
	slog.Debug("capturing frame", "worker", c.addr)
	// Placeholder — real implementation reads from worker capture API
	return &FrameData{Width: 320, Height: 200}, nil
}

// CaptureAudio retrieves the latest PCM audio buffer from the worker.
func (c *Client) CaptureAudio() (*AudioData, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed {
		return nil, fmt.Errorf("worker client closed")
	}
	slog.Debug("capturing audio", "worker", c.addr)
	return &AudioData{Rate: 11025, Channels: 2}, nil
}

// InjectInput sends an input event to the worker.
func (c *Client) InjectInput(evt InputEvent) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed {
		return fmt.Errorf("worker client closed")
	}
	slog.Debug("injecting input", "worker", c.addr, "event", evt)
	// Placeholder — real implementation writes to worker injection API
	return nil
}

// Close shuts down the worker connection.
func (c *Client) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.closed = true
	return nil
}
