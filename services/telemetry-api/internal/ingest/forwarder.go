// Package ingest forwards telemetry events to Azure Application Insights.
package ingest

import (
	"log/slog"
	"sync"
	"time"
)

// Event represents a telemetry event from any service.
type Event struct {
	Name       string            `json:"name"`
	Timestamp  time.Time         `json:"timestamp"`
	Properties map[string]string `json:"properties"`
	Metrics    map[string]float64 `json:"metrics,omitempty"`
	TraceID    string            `json:"traceId,omitempty"`
	SpanID     string            `json:"spanId,omitempty"`
	ParentID   string            `json:"parentId,omitempty"`
}

// AppInsightsForwarder sends telemetry to Application Insights.
// In production, this uses the Application Insights SDK or OTLP exporter.
type AppInsightsForwarder struct {
	connectionString string
	mu               sync.Mutex
	buffer           []Event
}

// NewAppInsightsForwarder creates a new forwarder.
func NewAppInsightsForwarder(connectionString string) *AppInsightsForwarder {
	return &AppInsightsForwarder{
		connectionString: connectionString,
		buffer:           make([]Event, 0, 100),
	}
}

// Send queues an event for forwarding to Application Insights.
func (f *AppInsightsForwarder) Send(evt Event) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.buffer = append(f.buffer, evt)

	// Auto-flush when buffer is large enough
	if len(f.buffer) >= 100 {
		f.flushLocked()
	}
}

// Flush sends all buffered events to Application Insights.
func (f *AppInsightsForwarder) Flush() {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.flushLocked()
}

func (f *AppInsightsForwarder) flushLocked() {
	if len(f.buffer) == 0 {
		return
	}
	slog.Info("flushing telemetry", "count", len(f.buffer))
	// Placeholder — real implementation sends to Application Insights ingestion endpoint
	// using the connection string for authentication
	f.buffer = f.buffer[:0]
}
