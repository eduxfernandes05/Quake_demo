// Package auth provides Microsoft Entra ID (Azure AD) authentication.
package auth

import (
	"context"
	"log/slog"
	"net/http"
	"strings"
)

type contextKey string

// UserIDKey is the context key for the authenticated user ID.
const UserIDKey contextKey = "userID"

// EntraIDProvider validates JWT tokens issued by Microsoft Entra ID.
type EntraIDProvider struct {
	TenantID string
	ClientID string
}

// NewEntraIDProvider creates a new Entra ID authentication provider.
func NewEntraIDProvider(tenantID, clientID string) *EntraIDProvider {
	return &EntraIDProvider{TenantID: tenantID, ClientID: clientID}
}

// Middleware returns an HTTP handler that validates the Authorization header
// and injects the authenticated user ID into the request context.
func (p *EntraIDProvider) Middleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		authHeader := r.Header.Get("Authorization")
		if authHeader == "" {
			http.Error(w, `{"error":"missing authorization header"}`, http.StatusUnauthorized)
			return
		}

		token := strings.TrimPrefix(authHeader, "Bearer ")
		if token == authHeader {
			http.Error(w, `{"error":"invalid authorization format"}`, http.StatusUnauthorized)
			return
		}

		// In production: validate JWT signature against Entra ID JWKS endpoint:
		//   https://login.microsoftonline.com/{tenantID}/discovery/v2.0/keys
		// Verify: issuer, audience (clientID), expiry, signature
		//
		// For now, extract a placeholder user ID from the token.
		userID, err := validateToken(token, p.TenantID, p.ClientID)
		if err != nil {
			slog.Warn("auth failed", "err", err)
			http.Error(w, `{"error":"invalid token"}`, http.StatusUnauthorized)
			return
		}

		ctx := context.WithValue(r.Context(), UserIDKey, userID)
		next.ServeHTTP(w, r.WithContext(ctx))
	}
}

// validateToken validates the JWT token and returns the subject (user ID).
// In production, this decodes and verifies the JWT against Entra ID keys.
func validateToken(token, tenantID, clientID string) (string, error) {
	// Placeholder — real implementation uses go-jose or similar library
	// to decode JWT, verify RS256 signature, check iss/aud/exp claims
	_ = tenantID
	_ = clientID

	// For development: accept any non-empty token
	if token == "" {
		return "", http.ErrNoCookie // reusing an error type
	}
	return "user-" + token[:minLen(len(token), 8)], nil
}

func minLen(a, b int) int {
	if a < b {
		return a
	}
	return b
}
