# Scaling Runbook

## Auto-Scaling Configuration

Game workers scale based on session demand. Each session requires one worker+gateway pair.

### Current Limits

| Service | Min Replicas | Max Replicas | Scale Trigger |
|---------|-------------|-------------|---------------|
| Game Worker | 0 | 100 | Session manager provisioning |
| Gateway | 0 | 100 | Paired with workers |
| Session Manager | 2 | 10 | HTTP concurrent requests |
| Assets API | 1 | 5 | HTTP concurrent requests |
| Telemetry API | 1 | 5 | HTTP concurrent requests |

## Manual Scaling

### Scale workers for expected load

```bash
# Pre-scale for an event (e.g., 50 concurrent sessions)
az containerapp update -n quake-dev-worker -g quake-dev \
  --min-replicas 50 --max-replicas 100
```

### Scale down after event

```bash
az containerapp update -n quake-dev-worker -g quake-dev \
  --min-replicas 0 --max-replicas 100
```

### Check current replica count

```bash
az containerapp replica list -n quake-dev-worker -g quake-dev -o table
```

## Load Testing

### Run load test (100 concurrent sessions)

```bash
# Using Azure Load Testing or k6
k6 run --vus 100 --duration 10m load-test.js
```

### Monitor during load test

```bash
# Watch real-time metrics
az monitor app-insights query \
  --app quake-dev-insights \
  --analytics-query "
    customMetrics
    | where timestamp > ago(5m)
    | where name in ('activeSessions', 'cpuUsage', 'memoryUsage')
    | summarize avg(value) by name, bin(timestamp, 1m)
    | order by timestamp desc
  "
```

## Capacity Planning

- Each worker uses ~1 vCPU and ~2 GiB RAM
- Each gateway uses ~0.5 vCPU and ~512 MiB RAM
- Network: ~5 Mbps per session (H.264 + Opus)
- Storage: ~50 MiB per game instance (PAK files)
