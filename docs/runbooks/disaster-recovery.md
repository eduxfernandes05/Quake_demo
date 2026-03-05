# Disaster Recovery Runbook

## Recovery Objectives

| Metric | Target | Notes |
|--------|--------|-------|
| RTO (Recovery Time Objective) | 1 hour | Time to restore service |
| RPO (Recovery Point Objective) | 0 | No persistent game state to lose |

## Failure Scenarios

### Scenario 1: Single Container Failure

**Impact:** One session affected
**Recovery:** Automatic via ACA health probes (< 30 seconds)

Container Apps automatically restarts unhealthy containers based on liveness probes.
No manual intervention required.

### Scenario 2: Container Apps Environment Failure

**Impact:** All sessions in the environment
**Recovery:** 15-30 minutes

```bash
# 1. Check environment status
az containerapp env show -n quake-dev-env -g quake-dev

# 2. If environment is unhealthy, redeploy
az deployment group create \
  --resource-group quake-dev \
  --template-file infra/main.bicep \
  --parameters infra/parameters/dev.bicepparam

# 3. Redeploy all container apps
az containerapp update -n quake-dev-worker -g quake-dev \
  --image quakedevacr.azurecr.io/quake-worker:latest
```

### Scenario 3: Regional Outage

**Impact:** Complete platform outage
**Recovery:** 30-60 minutes

```bash
# 1. Deploy to backup region
az deployment group create \
  --resource-group quake-dr \
  --template-file infra/main.bicep \
  --parameters @infra/parameters/dr.bicepparam

# 2. Update Front Door to route to DR region
az afd endpoint route update --endpoint-name quake-dev-endpoint \
  --profile-name quake-dev-fd -g quake-dev \
  --route-name api-route \
  --origin-group dr-session-origins

# 3. Upload game assets to DR storage
az storage file copy start-batch \
  --source-account-name quakedevstg \
  --destination-account-name quakedrstg \
  --destination-share gamedata
```

### Scenario 4: ACR Image Corruption

**Impact:** Cannot deploy new containers
**Recovery:** 15 minutes

```bash
# Rebuild and push images from source
az acr build -r quakedevacr -t quake-worker:latest .
cd gateway && az acr build -r quakedevacr -t gateway:latest .
cd session-manager && az acr build -r quakedevacr -t session-manager:latest .
```

## Backup Strategy

| Resource | Backup Method | Frequency | Retention |
|----------|--------------|-----------|-----------|
| Container Images | ACR geo-replication | On push | 30 days |
| Game Assets | Azure Files snapshots | Daily | 7 days |
| Configuration | Git repository | On commit | Unlimited |
| Secrets | Key Vault soft-delete | Automatic | 7 days |
| Logs | Log Analytics retention | Continuous | 30 days |
