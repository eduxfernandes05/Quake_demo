// Package main implements the session manager service entry point.
//
// The session manager handles:
//   - Microsoft Entra ID (Azure AD) OAuth 2.0 authentication
//   - REST API for session lifecycle (create / get / delete)
//   - Worker+gateway provisioning via ACA management API
//   - Session → worker routing (returns gateway WebSocket URL)
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

	"github.com/eduxfernandes05/Quake_demo/session-manager/internal/auth"
	"github.com/eduxfernandes05/Quake_demo/session-manager/internal/provisioner"
	"github.com/eduxfernandes05/Quake_demo/session-manager/internal/session"
)

func main() {
	logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo}))
	slog.SetDefault(logger)

	port := envOr("SESSION_MGR_PORT", "8082")
	tenantID := envOr("AZURE_TENANT_ID", "")
	clientID := envOr("AZURE_CLIENT_ID", "")

	slog.Info("starting session manager", "port", port)

	authProvider := auth.NewEntraIDProvider(tenantID, clientID)
	store := session.NewInMemoryStore()
	prov := provisioner.NewACAProvisioner(
		envOr("ACA_RESOURCE_GROUP", ""),
		envOr("ACA_ENVIRONMENT", ""),
		envOr("ACA_SUBSCRIPTION", ""),
	)

	mux := http.NewServeMux()

	// Health endpoint
	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
	})

	// Session API — protected by Entra ID
	mux.HandleFunc("POST /api/sessions", authProvider.Middleware(createSession(store, prov)))
	mux.HandleFunc("GET /api/sessions/{id}", authProvider.Middleware(getSession(store)))
	mux.HandleFunc("DELETE /api/sessions/{id}", authProvider.Middleware(deleteSession(store, prov)))

	srv := &http.Server{
		Addr:              ":" + port,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT)
	defer stop()

	go func() {
		slog.Info("session manager listening", "addr", srv.Addr)
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
	slog.Info("session manager stopped")
}

func createSession(store *session.InMemoryStore, prov *provisioner.ACAProvisioner) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		userID := r.Context().Value(auth.UserIDKey).(string)

		// Provision worker+gateway pair
		workerInfo, err := prov.Provision(r.Context(), userID)
		if err != nil {
			slog.Error("provision failed", "err", err)
			http.Error(w, `{"error":"provisioning failed"}`, http.StatusInternalServerError)
			return
		}

		sess := store.Create(userID, workerInfo.GatewayURL)

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusCreated)
		json.NewEncoder(w).Encode(sess)
	}
}

func getSession(store *session.InMemoryStore) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		id := r.PathValue("id")
		sess, ok := store.Get(id)
		if !ok {
			http.Error(w, `{"error":"not found"}`, http.StatusNotFound)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(sess)
	}
}

func deleteSession(store *session.InMemoryStore, prov *provisioner.ACAProvisioner) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		id := r.PathValue("id")
		sess, ok := store.Get(id)
		if !ok {
			http.Error(w, `{"error":"not found"}`, http.StatusNotFound)
			return
		}

		if err := prov.Deprovision(r.Context(), sess.ID); err != nil {
			slog.Error("deprovision failed", "err", err)
		}

		store.Delete(id)
		w.WriteHeader(http.StatusNoContent)
	}
}

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
