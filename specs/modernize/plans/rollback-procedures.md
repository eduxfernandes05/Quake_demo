# Rollback Procedures

## Principles

1. Every deployment is reversible within 15 minutes
2. Rollback does not require code changes — only infrastructure actions
3. Data migrations are backward-compatible (expand-contract pattern)
4. Rollback procedures are tested quarterly

---

## Container Application Rollback (ACA Revisions)

Azure Container Apps supports multiple active revisions with traffic splitting.

### Rollback to Previous Revision

```bash
# 1. List revisions for the app
az containerapp revision list -n <app-name> -g <resource-group> -o table

# 2. Identify the last-known-good revision
# Look for the revision before the current active one

# 3. Route 100% traffic to previous revision
az containerapp ingress traffic set \
  -n <app-name> -g <resource-group> \
  --revision-weight <previous-revision>=100

# 4. Deactivate the bad revision
az containerapp revision deactivate \
  -n <app-name> -g <resource-group> \
  --revision <bad-revision>
```

### Automated Rollback (Canary Failure)

The CI/CD pipeline monitors error rate during canary deployment. If error rate exceeds 5% over 5 minutes:

1. Pipeline automatically sets traffic weight on new revision to 0
2. Pipeline sets previous revision to 100% traffic
3. Pipeline deactivates new revision
4. Alert fires to operations team
5. Post-mortem investigation before retry

---

## Service-Specific Rollback

### Game Worker

| Scenario | Impact | Rollback Action |
| --- | --- | --- |
| Worker crashes on start | No new sessions | Revert ACA revision to previous image |
| Worker renders incorrectly | Visual artifacts | Revert ACA revision |
| Worker memory leak | Gradual degradation | Revert ACA revision + restart existing instances |
| Worker fails health check | ACA auto-restarts | If repeated: revert revision |

**Data considerations**: Game workers are stateless per session. No data migration needed.

### Streaming Gateway

| Scenario | Impact | Rollback Action |
| --- | --- | --- |
| Encoding failure | Black/corrupt stream | Revert gateway revision |
| WebRTC negotiation failure | Cannot connect | Revert gateway revision |
| High latency | Poor experience | Revert gateway revision + investigate |

**Note**: Gateway and worker are co-located. Rollback both together.

### Session Manager

| Scenario | Impact | Rollback Action |
| --- | --- | --- |
| Auth failure | Cannot create sessions | Revert session-manager revision |
| Database schema mismatch | Session creation fails | Revert revision + run backward migration |
| API breaking change | Client errors | Revert revision |

**Data considerations**: If database schema changed, run backward migration script before reverting. All schema changes must be backward-compatible (add columns, never remove/rename).

### Assets API

| Scenario | Impact | Rollback Action |
| --- | --- | --- |
| Asset serving failure | Missing game content | Revert revision |
| CDN cache poisoning | Stale/wrong assets | Purge CDN cache + revert revision |

**Data considerations**: Blob storage assets are versioned. Revert API to serve previous asset version.

### Telemetry API

| Scenario | Impact | Rollback Action |
| --- | --- | --- |
| Ingestion failure | Lost telemetry | Revert revision; events buffered by clients temporarily |
| Schema mismatch | Malformed events | Revert revision |

**Impact**: Telemetry loss is non-critical. Games continue without telemetry.

---

## Infrastructure Rollback

### Bicep Deployment Rollback

```bash
# 1. List deployment history
az deployment group list -g <resource-group> -o table

# 2. Identify last-known-good deployment
# Note the deployment name/timestamp

# 3. Re-deploy previous Bicep template
git checkout <previous-commit> -- infra/
az deployment group create \
  --resource-group <resource-group> \
  --template-file infra/main.bicep \
  --parameters infra/parameters/prod.bicepparam
```

### Emergency: Full Environment Teardown

If the entire ACA environment is compromised:

```bash
# 1. Redirect traffic away (Front Door)
az afd route update --disable

# 2. Delete the ACA environment (destroys all apps)
az containerapp env delete -n <env-name> -g <resource-group>

# 3. Re-provision from Bicep
az deployment group create --template-file infra/main.bicep ...

# 4. Re-deploy all services from last-known-good images in ACR
```

**Recovery time**: ~30 minutes for full re-provisioning.

---

## Database Rollback

### Cosmos DB (Session State)

- All schema changes are additive (new properties only)
- Documents stored with schema version field
- Rollback: deploy previous service version that reads older schema
- Emergency: TTL on session documents auto-cleans after 24h

### Blob Storage (Game Assets)

- Enable soft delete (14-day retention)
- Enable versioning on blob containers
- Rollback: restore previous blob version

---

## Rollback Decision Matrix

| Signal | Severity | Auto-Rollback | Manual Required |
| --- | --- | --- | --- |
| Error rate > 5% during canary | Sev 1 | Yes (pipeline) | No |
| Health check failure > 3x in 10 min | Sev 1 | Yes (ACA restart then revision rollback) | If persists |
| P95 latency > 500ms | Sev 2 | No | Yes — evaluate root cause |
| Security vulnerability discovered | Sev 1 | No | Yes — revert or hotfix |
| Cost anomaly > 200% | Sev 3 | No | Yes — investigate and potentially scale down |
| Data corruption | Sev 0 | No | Yes — full incident response |

---

## Rollback Testing

| Procedure | Frequency | Method |
| --- | --- | --- |
| ACA revision rollback | Monthly | Deploy known-bad revision to staging, rollback, verify |
| Bicep re-deployment | Quarterly | Destroy and re-provision staging from IaC |
| Database restore | Quarterly | Restore Cosmos DB from backup to staging |
| CDN cache purge | Monthly | Purge staging CDN, verify fresh asset serving |
| Full environment rebuild | Semi-annually | Full teardown and rebuild of staging environment |
