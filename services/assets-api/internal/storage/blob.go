// Package storage wraps Azure Blob Storage access.
package storage

import (
	"context"
	"fmt"
	"log/slog"
)

// BlobClient provides access to Azure Blob Storage.
// In production, this uses the Azure SDK for Go.
type BlobClient struct {
	Account   string
	Container string
}

// NewBlobClient creates a blob storage client.
func NewBlobClient(account, container string) *BlobClient {
	return &BlobClient{Account: account, Container: container}
}

// GetBlob retrieves a blob by path. Returns the content and content type.
// In production, this uses the Azure Blob Storage SDK.
func (c *BlobClient) GetBlob(ctx context.Context, path string) ([]byte, string, error) {
	slog.Debug("fetching blob", "account", c.Account, "container", c.Container, "path", path)
	// Placeholder — real implementation uses azblob SDK
	return nil, "", fmt.Errorf("blob %q not found", path)
}

// ListBlobs returns a list of blob names under the given prefix.
func (c *BlobClient) ListBlobs(ctx context.Context, prefix string) ([]string, error) {
	slog.Debug("listing blobs", "account", c.Account, "container", c.Container, "prefix", prefix)
	// Placeholder — real implementation uses azblob SDK
	return []string{}, nil
}
