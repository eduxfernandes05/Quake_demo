// Package main implements the streaming gateway entry point.
//
// The gateway sits between the headless Quake worker and the browser.
// It encodes captured frames (RGBA → H.264) and audio (PCM → Opus),
// delivers them over WebRTC, and relays browser input back to the
// worker's injection API.
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

	"github.com/eduxfernandes05/Quake_demo/gateway/internal/signaling"
	"github.com/eduxfernandes05/Quake_demo/gateway/internal/worker"
)

func main() {
	// Structured JSON logger
	logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo}))
	slog.SetDefault(logger)

	port := envOr("GATEWAY_PORT", "8081")
	workerAddr := envOr("WORKER_ADDR", "localhost:8080")

	slog.Info("starting gateway", "port", port, "worker", workerAddr)

	wc := worker.NewClient(workerAddr)

	mux := http.NewServeMux()

	// Health endpoint
	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
	})

	// WebRTC signaling endpoint
	sigHandler := signaling.NewHandler(wc)
	mux.HandleFunc("/ws/signaling", sigHandler.ServeHTTP)

	// Serve the browser client
	mux.Handle("/", http.FileServer(http.Dir("web")))

	srv := &http.Server{
		Addr:              ":" + port,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}

	// Graceful shutdown
	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	go func() {
		slog.Info("gateway listening", "addr", srv.Addr)
		if err := srv.ListenAndServe(); err != http.ErrServerClosed {
			slog.Error("server error", "err", err)
			os.Exit(1)
		}
	}()

	<-ctx.Done()
	slog.Info("shutdown signal received")

	shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := srv.Shutdown(shutdownCtx); err != nil {
		slog.Error("shutdown error", "err", err)
	}
	slog.Info("gateway stopped")
}

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
