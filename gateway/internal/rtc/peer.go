// Package rtc manages WebRTC peer connections for streaming.
//
// Implements a lightweight SFU that:
//   - Sends H.264 video via RTP
//   - Sends Opus audio via RTP
//   - Receives browser input via a DataChannel
package rtc

import (
	"encoding/json"
	"log/slog"
	"sync"

	"github.com/eduxfernandes05/Quake_demo/gateway/internal/worker"
)

// PeerConnection wraps a single WebRTC peer.
type PeerConnection struct {
	ID       string
	workerCl *worker.Client
	mu       sync.Mutex
	closed   bool
}

// NewPeerConnection creates a new WebRTC peer connection.
// In production this uses pion/webrtc for ICE, DTLS, SRTP.
func NewPeerConnection(id string, wc *worker.Client) *PeerConnection {
	slog.Info("creating peer connection", "id", id)
	return &PeerConnection{ID: id, workerCl: wc}
}

// HandleOffer processes an SDP offer from the browser and returns an answer.
func (pc *PeerConnection) HandleOffer(offerSDP string) (string, error) {
	pc.mu.Lock()
	defer pc.mu.Unlock()

	slog.Info("handling SDP offer", "peer", pc.ID)

	// Placeholder — real implementation:
	// 1. Create pion.PeerConnection
	// 2. Add H.264 video track
	// 3. Add Opus audio track
	// 4. Create data channel for input
	// 5. Set remote description (offer)
	// 6. Create and return answer

	answer := map[string]string{
		"type": "answer",
		"sdp":  "v=0\r\n...",
	}
	data, _ := json.Marshal(answer)
	return string(data), nil
}

// AddICECandidate adds a trickle ICE candidate from the browser.
func (pc *PeerConnection) AddICECandidate(candidate string) error {
	slog.Debug("adding ICE candidate", "peer", pc.ID)
	return nil
}

// Close tears down the peer connection.
func (pc *PeerConnection) Close() error {
	pc.mu.Lock()
	defer pc.mu.Unlock()
	if pc.closed {
		return nil
	}
	pc.closed = true
	slog.Info("peer connection closed", "id", pc.ID)
	return nil
}
