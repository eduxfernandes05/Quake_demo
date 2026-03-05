// Package main implements the telemetry API service.
//
// The telemetry API ingests events from all services and forwards them
// to Azure Application Insights. It also exposes OpenTelemetry endpoints
// for distributed tracing with W3C context propagation.
package main

import (
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/eduxfernandes05/Quake_demo/services/telemetry-api/internal/handler"
	"github.com/eduxfernandes05/Quake_demo/services/telemetry-api/internal/ingest"
)

func main() {
	logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo}))
	slog.SetDefault(logger)

	port := envOr("TELEMETRY_API_PORT", "8084")
	appInsightsConn := envOr("APPLICATIONINSIGHTS_CONNECTION_STRING", "")

	slog.Info("starting telemetry API", "port", port)

	forwarder := ingest.NewAppInsightsForwarder(appInsightsConn)
	telemetryHandler := handler.NewTelemetryHandler(forwarder)

	mux := http.NewServeMux()

	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
	})

	mux.HandleFunc("POST /api/events", telemetryHandler.IngestEvent)
	mux.HandleFunc("POST /api/traces", telemetryHandler.IngestTrace)
	mux.HandleFunc("GET /api/metrics", telemetryHandler.GetMetrics)

	srv := &http.Server{
		Addr:              ":" + port,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	go func() {
		slog.Info("telemetry API listening", "addr", srv.Addr)
		if err := srv.ListenAndServe(); err != http.ErrServerClosed {
			slog.Error("server error", "err", err)
			os.Exit(1)
		}
	}()

	<-ctx.Done()
	slog.Info("shutdown signal received")
	shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	srv.Shutdown(shutdownCtx)
	forwarder.Flush()
	slog.Info("telemetry API stopped")
}

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
