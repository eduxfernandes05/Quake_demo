// Package handler implements HTTP handlers for the assets API.
package handler

import (
	"encoding/json"
	"net/http"

	"github.com/eduxfernandes05/Quake_demo/services/assets-api/internal/storage"
)

// AssetHandler serves game asset requests.
type AssetHandler struct {
	blob *storage.BlobClient
}

// NewAssetHandler creates an asset handler.
func NewAssetHandler(blob *storage.BlobClient) *AssetHandler {
	return &AssetHandler{blob: blob}
}

// GetAsset retrieves a single game asset by path.
func (h *AssetHandler) GetAsset(w http.ResponseWriter, r *http.Request) {
	path := r.PathValue("path")
	if path == "" {
		http.Error(w, `{"error":"path required"}`, http.StatusBadRequest)
		return
	}

	data, contentType, err := h.blob.GetBlob(r.Context(), path)
	if err != nil {
		http.Error(w, `{"error":"not found"}`, http.StatusNotFound)
		return
	}

	if contentType != "" {
		w.Header().Set("Content-Type", contentType)
	}
	w.Header().Set("Cache-Control", "public, max-age=86400")
	w.Write(data)
}

// ListAssets returns a JSON list of available assets.
func (h *AssetHandler) ListAssets(w http.ResponseWriter, r *http.Request) {
	prefix := r.URL.Query().Get("prefix")
	blobs, err := h.blob.ListBlobs(r.Context(), prefix)
	if err != nil {
		http.Error(w, `{"error":"listing failed"}`, http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{"assets": blobs})
}
