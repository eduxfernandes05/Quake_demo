// Package session manages game session lifecycle and state.
package session

import (
	"crypto/rand"
	"encoding/hex"
	"sync"
	"time"
)

// Session represents a single game session.
type Session struct {
	ID         string    `json:"id"`
	UserID     string    `json:"userId"`
	GatewayURL string    `json:"gatewayUrl"`
	State      string    `json:"state"` // "active", "terminated"
	CreatedAt  time.Time `json:"createdAt"`
}

// InMemoryStore is a simple in-memory session store.
// In production this would be backed by Azure Cosmos DB or Redis.
type InMemoryStore struct {
	mu       sync.RWMutex
	sessions map[string]*Session
}

// NewInMemoryStore creates an empty session store.
func NewInMemoryStore() *InMemoryStore {
	return &InMemoryStore{sessions: make(map[string]*Session)}
}

// Create adds a new session and returns it.
func (s *InMemoryStore) Create(userID, gatewayURL string) *Session {
	s.mu.Lock()
	defer s.mu.Unlock()

	sess := &Session{
		ID:         generateID(),
		UserID:     userID,
		GatewayURL: gatewayURL,
		State:      "active",
		CreatedAt:  time.Now().UTC(),
	}
	s.sessions[sess.ID] = sess
	return sess
}

// Get retrieves a session by ID.
func (s *InMemoryStore) Get(id string) (*Session, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	sess, ok := s.sessions[id]
	return sess, ok
}

// Delete removes a session by ID.
func (s *InMemoryStore) Delete(id string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.sessions, id)
}

// ListByUser returns all sessions for a given user.
func (s *InMemoryStore) ListByUser(userID string) []*Session {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []*Session
	for _, sess := range s.sessions {
		if sess.UserID == userID {
			result = append(result, sess)
		}
	}
	return result
}

func generateID() string {
	b := make([]byte, 16)
	rand.Read(b)
	return hex.EncodeToString(b)
}
