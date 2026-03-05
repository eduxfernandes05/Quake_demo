# DevOps Transformation Strategy

## Current State: Level 0 — No DevOps

- No version control (source code is a file dump)
- No build automation (MSVC 6.0 project files)
- No testing of any kind
- No CI/CD pipeline
- No infrastructure as code
- No monitoring or observability
- No deployment automation

## Target State: Level 3 — Mature DevOps

### Source Control

**Git + GitHub**:
- Trunk-based development, short-lived feature branches
- Protected `main` branch: require PR, CI pass, 1 review
- Conventional commits for changelog generation
- `.gitignore` excluding build artifacts, `.Zone.Identifier` files, game assets

### Build Automation

**CMake + Docker Multi-Stage**:
```
Source → CMake → Linux binary → Docker image → ACR
```

- `CMakeLists.txt` for cross-platform C build
- Multi-stage Dockerfile: builder (compile) + runtime (minimal image)
- Build matrix: Debug + Release, AddressSanitizer + standard
- Deterministic builds: pinned base images, locked dependencies

### CI Pipeline (GitHub Actions)

```yaml
# Triggered on: push to main, pull_request
jobs:
  build:
    - Checkout
    - CMake configure + build (Debug + Release)
    - Run unit tests
    - Run AddressSanitizer build + tests
    - Static analysis (cppcheck, clang-tidy)
  
  container:
    - Docker build (game-worker image)
    - Trivy vulnerability scan
    - Push to ACR (tagged with git SHA + branch)
  
  services:
    - Build streaming-gateway image
    - Build session-manager image
    - Build assets-api image
    - Build telemetry-api image
    - Trivy scan all images
    - Push all to ACR
  
  integration-test:
    - Deploy to dev ACA environment
    - Run integration test suite
    - Run smoke tests (session create → stream → destroy)
```

### CD Pipeline (GitHub Actions)

```yaml
# Triggered on: push to main (after CI passes)
jobs:
  deploy-staging:
    - Deploy all ACA revisions to staging
    - Run E2E tests against staging
    - Performance benchmark (latency, throughput)
  
  deploy-production:
    - Canary deployment: route 10% traffic to new revision
    - Monitor error rate, latency for 15 minutes
    - If healthy: progressive rollout to 100%
    - If unhealthy: automatic rollback to previous revision
```

### Infrastructure as Code

**Bicep + Azure Developer CLI (azd)**:

```
infra/
├── main.bicep                    # Root orchestration
├── modules/
│   ├── container-apps-env.bicep  # ACA environment + VNet
│   ├── container-registry.bicep  # ACR
│   ├── container-app.bicep       # Generic ACA app module
│   ├── key-vault.bicep           # Key Vault + access policies
│   ├── storage.bicep             # Azure Files + Blob Storage
│   ├── front-door.bicep          # Front Door + WAF
│   ├── monitoring.bicep          # App Insights + Log Analytics
│   ├── cosmos-db.bicep           # Session state store
│   └── entra-app.bicep           # App registration
├── parameters/
│   ├── dev.bicepparam
│   ├── staging.bicepparam
│   └── prod.bicepparam
└── azure.yaml                    # azd project definition
```

**Key Principles**:
- All resources defined in Bicep — no portal click-ops
- Parameterized per environment (dev / staging / prod)
- RBAC assignments in Bicep (managed identity → Key Vault, Storage)
- `azd up` for full provisioning + deployment

### Environments

| Environment | Purpose | Scale | Lifecycle |
| --- | --- | --- | --- |
| dev | Development and debugging | 1 worker, no CDN | On-demand, destroyed nightly |
| staging | Pre-production validation | 3 workers, full stack | Persistent, reset weekly |
| prod | Live service | Auto-scaled, full HA | Persistent |

### Monitoring and Observability

**Three Pillars**:

1. **Logs** (Azure Monitor / Log Analytics):
   - All services emit structured JSON to stdout
   - ACA log driver → Log Analytics workspace
   - KQL queries for troubleshooting and investigation
   - 90-day retention

2. **Metrics** (Azure Application Insights):
   - Custom metrics: session count, frame latency, encoding time, input latency
   - Standard ACA metrics: CPU, memory, request count, response time
   - Dashboard: Azure Monitor Workbooks

3. **Traces** (OpenTelemetry → App Insights):
   - Distributed tracing across all 5 services
   - Trace context propagated via W3C Trace Context headers
   - End-to-end: session creation → worker start → frame encode → browser display

**Dashboards**:
- **Operations**: Active sessions, worker utilization, error rates, latency P50/P95/P99
- **Player Experience**: Input-to-display latency, stream quality, session duration
- **Cost**: Active containers, compute hours, bandwidth, storage
- **Security**: Failed auth attempts, WAF blocks, anomalous patterns

**Alerting Rules**:
| Alert | Condition | Severity |
| --- | --- | --- |
| High error rate | >5% 5xx responses over 5 min | Sev 1 |
| High latency | P95 > 200ms input-to-display | Sev 2 |
| Worker crash loop | >3 restarts in 10 min | Sev 1 |
| Capacity exhaustion | Available workers < 10% | Sev 2 |
| Auth failures spike | >50 401s per minute | Sev 2 |
| Cost anomaly | Daily spend > 150% of 7-day average | Sev 3 |

### Testing Strategy Overview

| Level | Tool | Scope |
| --- | --- | --- |
| Unit tests | Custom C test harness or CMocka | Game engine functions, input sanitization |
| Integration tests | Docker Compose + pytest | Worker + gateway interaction |
| E2E tests | Playwright + custom WebRTC client | Full browser session lifecycle |
| Performance tests | k6 or custom load generator | Session density, latency under load |
| Security tests | Trivy (containers), OWASP ZAP (APIs) | Vulnerability scanning |
| Chaos tests | Azure Chaos Studio (future) | Fault injection |

### Operational Runbooks

Create runbooks for:
- **Incident**: High error rate — investigation and remediation steps
- **Scaling**: Manual capacity override procedures
- **Deployment**: Rollback procedures for failed deployments
- **Security**: Compromised container response
- **Cost**: Runaway cost investigation and mitigation
- **Recovery**: Restore from backup procedures

### DevOps Maturity Progression

| Milestone | Level | Key Capabilities |
| --- | --- | --- |
| Phase 0 complete | Level 1 | Git, CI builds, container images, basic Bicep |
| Phase 1 complete | Level 1.5 | Health checks, structured logs, Docker compose local dev |
| Phase 2 complete | Level 2 | Multi-service CI, integration tests, staging env |
| Phase 3 complete | Level 2.5 | Auth, session management, canary deploys |
| Phase 5 complete | Level 3 | Full monitoring, alerting, security scanning, runbooks |
