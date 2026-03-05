# Incident Response Runbook

## Severity Levels

| Level | Description | Response Time | Example |
|-------|-------------|---------------|---------|
| SEV-1 | Complete outage | 15 min | All sessions down, no new connections |
| SEV-2 | Major degradation | 30 min | >50% sessions affected, high error rate |
| SEV-3 | Minor issue | 2 hours | Single service degraded, workaround exists |
| SEV-4 | Low impact | Next business day | Cosmetic issue, non-critical alert |

## Response Procedure

### 1. Detection & Triage

```bash
# Check overall platform health
az containerapp list -g quake-dev --query "[].{name:name, status:properties.runningStatus}" -o table

# Check Application Insights for error spikes
az monitor app-insights query \
  --app quake-dev-insights \
  --analytics-query "exceptions | where timestamp > ago(15m) | summarize count() by type"
```

### 2. Containment

For game worker issues:
```bash
# Scale down affected workers
az containerapp update -n quake-dev-worker -g quake-dev --min-replicas 0

# Check worker logs
az containerapp logs show -n quake-dev-worker -g quake-dev --tail 100
```

For gateway issues:
```bash
# Restart gateway containers
az containerapp revision restart -n quake-dev-gateway -g quake-dev --revision <revision>
```

### 3. Communication

- Update status page
- Notify on-call team via PagerDuty/OpsGenie
- Post updates every 30 minutes for SEV-1/2

### 4. Resolution & Post-mortem

- Deploy fix or rollback (see [deployment.md](deployment.md))
- Verify health endpoints return 200
- Monitor for 30 minutes after fix
- Schedule post-mortem within 48 hours
