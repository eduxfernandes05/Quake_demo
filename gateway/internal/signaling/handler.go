// Package signaling implements WebSocket-based WebRTC signaling.
//
// Protocol (JSON over WebSocket):
//
//	Client → Server: {"type":"offer", "sdp":"..."}
//	Server → Client: {"type":"answer", "sdp":"..."}
//	Client → Server: {"type":"candidate", "candidate":"..."}
//	Server → Client: {"type":"candidate", "candidate":"..."}
package signaling

import (
	"encoding/json"
	"log/slog"
	"net/http"
	"sync"

	"github.com/eduxfernandes05/Quake_demo/gateway/internal/rtc"
	"github.com/eduxfernandes05/Quake_demo/gateway/internal/worker"
)

// Message is the signaling wire format.
type Message struct {
	Type      string `json:"type"`
	SDP       string `json:"sdp,omitempty"`
	Candidate string `json:"candidate,omitempty"`
}

// Handler serves the WebSocket signaling endpoint.
type Handler struct {
	workerCl *worker.Client
	mu       sync.Mutex
	peers    map[string]*rtc.PeerConnection
	nextID   int
}

// NewHandler creates a signaling handler connected to the given worker.
func NewHandler(wc *worker.Client) *Handler {
	return &Handler{
		workerCl: wc,
		peers:    make(map[string]*rtc.PeerConnection),
	}
}

// ServeHTTP upgrades the connection to WebSocket and handles signaling.
// In production this uses gorilla/websocket or nhooyr.io/websocket.
// Here we implement a simplified HTTP-based signaling fallback so the
// code compiles and the structure is clear.
func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodPost {
		h.handlePost(w, r)
		return
	}

	// For GET, return endpoint info
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{
		"status":   "ok",
		"protocol": "websocket",
		"info":     "POST an SDP offer to begin a session",
	})
}

func (h *Handler) handlePost(w http.ResponseWriter, r *http.Request) {
	var msg Message
	if err := json.NewDecoder(r.Body).Decode(&msg); err != nil {
		http.Error(w, `{"error":"invalid json"}`, http.StatusBadRequest)
		return
	}

	switch msg.Type {
	case "offer":
		h.mu.Lock()
		h.nextID++
		peerID := peerIDString(h.nextID)
		pc := rtc.NewPeerConnection(peerID, h.workerCl)
		h.peers[peerID] = pc
		h.mu.Unlock()

		answer, err := pc.HandleOffer(msg.SDP)
		if err != nil {
			slog.Error("offer handling failed", "err", err)
			http.Error(w, `{"error":"offer failed"}`, http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(answer))

	case "candidate":
		// In production, route to the correct peer
		slog.Debug("received ICE candidate")
		w.WriteHeader(http.StatusNoContent)

	default:
		http.Error(w, `{"error":"unknown message type"}`, http.StatusBadRequest)
	}
}

func peerIDString(n int) string {
	return "peer-" + itoa(n)
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	var buf [20]byte
	i := len(buf) - 1
	for n > 0 {
		buf[i] = byte('0' + n%10)
		n /= 10
		i--
	}
	return string(buf[i+1:])
}
