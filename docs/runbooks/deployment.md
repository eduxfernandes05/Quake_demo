# Deployment Runbook

## Canary Deployment Procedure

Azure Container Apps supports traffic splitting between revisions for canary deployments.

### 1. Build and Push New Image

```bash
# Build the new version
az acr build -r quakedevacr -t quake-worker:v2.0.0 .

# Verify image was pushed
az acr repository show-tags -n quakedevacr --repository quake-worker
```

### 2. Create New Revision (Canary)

```bash
# Deploy new revision with 0% traffic
az containerapp update -n quake-dev-worker -g quake-dev \
  --image quakedevacr.azurecr.io/quake-worker:v2.0.0 \
  --revision-suffix v2

# Split traffic: 90% old, 10% canary
az containerapp ingress traffic set -n quake-dev-worker -g quake-dev \
  --revision-weight quake-dev-worker--v1=90 quake-dev-worker--v2=10
```

### 3. Monitor Canary

```bash
# Watch error rate by revision
az monitor app-insights query \
  --app quake-dev-insights \
  --analytics-query "
    requests
    | where timestamp > ago(30m)
    | summarize errorRate=countif(success == false) * 100.0 / count() by cloud_RoleName
  "

# Check health of canary revision
az containerapp revision show -n quake-dev-worker -g quake-dev --revision quake-dev-worker--v2
```

### 4. Promote or Rollback

**Promote (if healthy):**
```bash
# Route 100% to new revision
az containerapp ingress traffic set -n quake-dev-worker -g quake-dev \
  --revision-weight quake-dev-worker--v2=100

# Deactivate old revision
az containerapp revision deactivate -n quake-dev-worker -g quake-dev \
  --revision quake-dev-worker--v1
```

**Rollback (if issues detected):**
```bash
# Route 100% back to old revision
az containerapp ingress traffic set -n quake-dev-worker -g quake-dev \
  --revision-weight quake-dev-worker--v1=100

# Deactivate failed revision
az containerapp revision deactivate -n quake-dev-worker -g quake-dev \
  --revision quake-dev-worker--v2
```

## CI/CD Pipeline

The deployment is automated via GitHub Actions:

1. Push to `main` → Build & test
2. Push tag `v*` → Build, push to ACR, canary deploy
3. Manual approval → Promote canary to full traffic
