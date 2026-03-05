# Task: Architecture Refactor — Session Manager Service

**Phase**: 3 (Session Management)  
**Priority**: P0  
**Estimated Effort**: 8–10 days  
**Prerequisites**: Game worker + streaming gateway running in ACA

## Objective

Build the session manager service that handles user authentication (Microsoft Entra ID), game session lifecycle (create, join, leave, destroy), and game worker provisioning on Azure Container Apps.

## Acceptance Criteria

- [ ] REST API with endpoints: POST/GET/DELETE `/api/sessions`
- [ ] Authentication via Microsoft Entra ID (OAuth 2.0 / OIDC)
- [ ] Unauthorized requests return 401
- [ ] Session creation provisions a game worker + gateway pair in ACA
- [ ] Session response includes WebSocket URL for streaming gateway
- [ ] Session destruction deprovisions the worker + gateway
- [ ] Session ownership enforced (user A cannot delete user B's session)
- [ ] Capacity limits enforced (503 when at maximum)
- [ ] Session state persisted in Azure Cosmos DB (or Redis)
- [ ] Idle sessions auto-cleaned after 15 minutes
- [ ] Health endpoint at `/healthz`
- [ ] Deployed as 2+ replicas for HA

## API Specification

```
POST   /api/sessions
  Headers: Authorization: Bearer <jwt>
  Body: { "map": "e1m1", "skill": 1 }
  Response 201: { "id": "abc123", "status": "starting", "websocketUrl": null }
  Response 401: Unauthorized
  Response 429: Too many sessions
  Response 503: At capacity

GET    /api/sessions/{id}
  Headers: Authorization: Bearer <jwt>
  Response 200: { "id": "abc123", "status": "running", "websocketUrl": "wss://..." }
  Response 403: Not your session
  Response 404: Session not found

DELETE /api/sessions/{id}
  Headers: Authorization: Bearer <jwt>
  Response 204: Session terminated
  Response 403: Not your session

GET    /api/sessions
  Headers: Authorization: Bearer <jwt>
  Response 200: [{ "id": "abc123", "status": "running", ... }]

GET    /api/capacity
  Response 200: { "available": 15, "total": 20, "inUse": 5 }
```

## Implementation Steps

### 1. Project Scaffold (C# / .NET 8)

```
session-manager/
├── Program.cs
├── Controllers/
│   └── SessionsController.cs
├── Services/
│   ├── SessionService.cs        # Session lifecycle logic
│   ├── WorkerProvisioningService.cs  # ACA container management
│   └── CapacityService.cs       # Capacity tracking
├── Models/
│   ├── Session.cs
│   └── SessionRequest.cs
├── Middleware/
│   └── AuthMiddleware.cs        # JWT validation
├── appsettings.json
├── Dockerfile
└── session-manager.csproj
```

### 2. Authentication

- Register application in Microsoft Entra ID
- Configure OAuth 2.0 authorization code flow with PKCE (browser client)
- Validate JWT tokens in middleware:
  - Check `iss` (Entra ID tenant)
  - Check `aud` (app client ID)
  - Check `exp` (not expired)
  - Extract `sub` or `oid` as user identity

### 3. Session Lifecycle

```
Create Session:
1. Validate JWT → extract user ID
2. Check user's active session count (max 1-3 per user)
3. Check global capacity
4. Create session record in Cosmos DB (status: "provisioning")
5. Trigger worker+gateway provisioning (ACA revision scale-up)
6. Return session ID (client polls for status)

Poll Session:
1. Client polls GET /api/sessions/{id}
2. When worker is healthy and gateway ready → status: "running", websocketUrl populated

Destroy Session:
1. Validate ownership
2. Scale down worker+gateway containers
3. Update session record (status: "terminated")
4. Cleanup after short delay
```

### 4. Worker Provisioning

Use Azure Container Apps Management SDK:
```csharp
// Scale a container app revision
var client = new ContainerAppsClient(credential);
await client.CreateOrUpdateAsync(
    resourceGroupName,
    workerAppName,
    new ContainerAppData { /* config with session-specific env vars */ }
);
```

Alternative: Use KEDA scaling based on session count metric.

### 5. Session State Store

Azure Cosmos DB (or Azure Cache for Redis):
```json
{
    "id": "session-abc123",
    "userId": "entra-oid-xxx",
    "status": "running",
    "map": "e1m1",
    "skill": 1,
    "workerContainerName": "worker-abc123",
    "gatewayUrl": "wss://gateway-abc123.aca-env.region.azurecontainerapps.io",
    "createdAt": "2024-01-15T10:30:00Z",
    "lastActiveAt": "2024-01-15T11:00:00Z",
    "ttl": 900
}
```

### 6. Idle Cleanup

Background task:
- Every 60 seconds, query sessions where `lastActiveAt` > 15 minutes ago
- For each: destroy session (deprovision workers)
- Cosmos DB TTL auto-deletes records after 24 hours

## Files to Create

Entire `session-manager/` project directory.

## Dependencies

- .NET 8 SDK
- Microsoft.Identity.Web (Entra ID integration)
- Azure.ResourceManager.AppContainers (ACA management)
- Microsoft.Azure.Cosmos (session state)

## Validation

```bash
# 1. Deploy to ACA
az containerapp create -n session-manager ...

# 2. Get auth token
TOKEN=$(curl -s "https://login.microsoftonline.com/..." | jq -r .access_token)

# 3. Create session
curl -X POST https://session-manager.aca.../api/sessions \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"map":"e1m1","skill":1}'

# 4. Poll until running
curl https://session-manager.aca.../api/sessions/abc123 \
  -H "Authorization: Bearer $TOKEN"
# Expect: status=running, websocketUrl present

# 5. Connect browser to websocketUrl → play

# 6. Destroy session
curl -X DELETE https://session-manager.aca.../api/sessions/abc123 \
  -H "Authorization: Bearer $TOKEN"
```

## Risks

- ACA provisioning latency: creating a new container app takes 10-30s. Use pre-warmed pool.
- Entra ID token validation: cache signing keys to avoid rate limiting.
- Session leak: if cleanup fails, orphaned workers consume resources. Implement belt-and-suspenders cleanup.

## Rollback

Session manager is independent service. Remove without affecting worker/gateway (they can run standalone).
