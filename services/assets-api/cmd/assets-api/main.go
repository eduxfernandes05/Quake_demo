// Package main implements the assets API service.
//
// The assets API serves extracted PAK file contents from Azure Blob Storage + CDN.
// It exposes a REST endpoint for retrieving game assets by path.
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

	"github.com/eduxfernandes05/Quake_demo/services/assets-api/internal/handler"
	"github.com/eduxfernandes05/Quake_demo/services/assets-api/internal/storage"
)

func main() {
	logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo}))
	slog.SetDefault(logger)

	port := envOr("ASSETS_API_PORT", "8083")
	storageAccount := envOr("AZURE_STORAGE_ACCOUNT", "")
	containerName := envOr("AZURE_STORAGE_CONTAINER", "game-assets")

	slog.Info("starting assets API", "port", port)

	blobClient := storage.NewBlobClient(storageAccount, containerName)
	assetHandler := handler.NewAssetHandler(blobClient)

	mux := http.NewServeMux()

	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
	})

	mux.HandleFunc("GET /api/assets/{path...}", assetHandler.GetAsset)
	mux.HandleFunc("GET /api/assets", assetHandler.ListAssets)

	srv := &http.Server{
		Addr:              ":" + port,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	go func() {
		slog.Info("assets API listening", "addr", srv.Addr)
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
	slog.Info("assets API stopped")
}

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
