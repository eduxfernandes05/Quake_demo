// Package provisioner manages worker+gateway container lifecycle in Azure Container Apps.
package provisioner

import (
	"context"
	"log/slog"
)

// WorkerInfo contains the details of a provisioned worker+gateway pair.
type WorkerInfo struct {
	WorkerID   string `json:"workerId"`
	GatewayURL string `json:"gatewayUrl"`
}

// ACAProvisioner uses the Azure Container Apps management API to
// scale worker+gateway pairs for individual sessions.
type ACAProvisioner struct {
	ResourceGroup string
	Environment   string
	Subscription  string
}

// NewACAProvisioner creates a new ACA provisioner.
func NewACAProvisioner(resourceGroup, environment, subscription string) *ACAProvisioner {
	return &ACAProvisioner{
		ResourceGroup: resourceGroup,
		Environment:   environment,
		Subscription:  subscription,
	}
}

// Provision creates a new worker+gateway pair for the given user.
// In production, this calls the ACA management API to create a new
// container app revision or scale up an existing one.
func (p *ACAProvisioner) Provision(ctx context.Context, userID string) (*WorkerInfo, error) {
	slog.Info("provisioning worker", "user", userID, "rg", p.ResourceGroup)

	// Placeholder — real implementation:
	// 1. Call ACA API to create/scale worker container
	// 2. Wait for worker to become healthy
	// 3. Return the gateway endpoint URL

	return &WorkerInfo{
		WorkerID:   "worker-" + userID,
		GatewayURL: "wss://gateway.example.com/ws/signaling",
	}, nil
}

// Deprovision removes the worker+gateway pair for the given session.
func (p *ACAProvisioner) Deprovision(ctx context.Context, sessionID string) error {
	slog.Info("deprovisioning worker", "session", sessionID, "rg", p.ResourceGroup)

	// Placeholder — real implementation:
	// 1. Call ACA API to scale down or delete the container app revision
	// 2. Clean up any associated resources

	return nil
}
