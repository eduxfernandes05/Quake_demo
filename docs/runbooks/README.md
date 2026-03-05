# Operational Runbooks

This directory contains operational procedures for the Quake Cloud Streaming Platform.

## Runbook Index

| Runbook | Description | When to Use |
|---------|-------------|-------------|
| [incident-response.md](incident-response.md) | Platform incident response procedure | Service outage or degradation |
| [scaling.md](scaling.md) | Manual and auto-scaling operations | Load changes, capacity planning |
| [deployment.md](deployment.md) | Canary deployment and rollback procedures | New releases |
| [disaster-recovery.md](disaster-recovery.md) | Backup restoration and DR failover | Regional outage, data loss |

## Quick Reference

### Health Check URLs

| Service | Endpoint | Expected Response |
|---------|----------|-------------------|
| Game Worker | `http://<worker>:8080/healthz` | `{"status":"ok"}` |
| Gateway | `http://<gateway>:8081/healthz` | `{"status":"ok"}` |
| Session Manager | `http://<session-mgr>:8082/healthz` | `{"status":"ok"}` |
| Assets API | `http://<assets>:8083/healthz` | `{"status":"ok"}` |
| Telemetry API | `http://<telemetry>:8084/healthz` | `{"status":"ok"}` |

### Key Azure Resources

| Resource | Type | Purpose |
|----------|------|---------|
| `quake-dev-env` | Container Apps Environment | Hosts all containers |
| `quake-dev-kv` | Key Vault | Secrets and certificates |
| `quakedevacr` | Container Registry | Container images |
| `quakedevstg` | Storage Account | Game assets (Azure Files) |
| `quake-dev-logs` | Log Analytics | Centralized logging |
| `quake-dev-insights` | Application Insights | APM and distributed tracing |
| `quake-dev-fd` | Front Door | Global load balancing + WAF |
