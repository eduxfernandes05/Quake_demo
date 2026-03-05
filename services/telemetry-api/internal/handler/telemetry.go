// Package handler implements HTTP handlers for the telemetry API.
package handler

import (
	"encoding/json"
	"net/http"
	"time"

	"github.com/eduxfernandes05/Quake_demo/services/telemetry-api/internal/ingest"
)

// TelemetryHandler processes telemetry ingestion requests.
type TelemetryHandler struct {
	forwarder *ingest.AppInsightsForwarder
}

// NewTelemetryHandler creates a telemetry handler.
func NewTelemetryHandler(fwd *ingest.AppInsightsForwarder) *TelemetryHandler {
	return &TelemetryHandler{forwarder: fwd}
}

// IngestEvent handles POST /api/events — ingests a telemetry event.
func (h *TelemetryHandler) IngestEvent(w http.ResponseWriter, r *http.Request) {
	var evt ingest.Event
	if err := json.NewDecoder(r.Body).Decode(&evt); err != nil {
		http.Error(w, `{"error":"invalid json"}`, http.StatusBadRequest)
		return
	}

	if evt.Timestamp.IsZero() {
		evt.Timestamp = time.Now().UTC()
	}

	// Propagate W3C trace context if present
	traceParent := r.Header.Get("traceparent")
	if traceParent != "" && evt.TraceID == "" {
		if evt.Properties == nil {
			evt.Properties = make(map[string]string)
		}
		evt.Properties["traceparent"] = traceParent
	}

	h.forwarder.Send(evt)

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusAccepted)
	json.NewEncoder(w).Encode(map[string]string{"status": "accepted"})
}

// IngestTrace handles POST /api/traces — ingests OpenTelemetry trace data.
func (h *TelemetryHandler) IngestTrace(w http.ResponseWriter, r *http.Request) {
	var evt ingest.Event
	if err := json.NewDecoder(r.Body).Decode(&evt); err != nil {
		http.Error(w, `{"error":"invalid json"}`, http.StatusBadRequest)
		return
	}

	evt.Timestamp = time.Now().UTC()
	h.forwarder.Send(evt)

	w.WriteHeader(http.StatusAccepted)
	json.NewEncoder(w).Encode(map[string]string{"status": "accepted"})
}

// GetMetrics handles GET /api/metrics — returns current telemetry metrics.
func (h *TelemetryHandler) GetMetrics(w http.ResponseWriter, r *http.Request) {
	// Placeholder — in production, query from Application Insights or local aggregator
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"activeSessions":   0,
		"totalEvents":      0,
		"avgLatencyMs":     0,
		"uptimeSeconds":    0,
	})
}
