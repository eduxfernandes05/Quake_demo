# Compliance Gaps Assessment

## Standards and Best Practice Gaps

### 1. Secure Software Development (OWASP / SDL)

| Practice | Current State | Gap |
| --- | --- | --- |
| Input validation | None. All network data trusted. | CRITICAL — must validate all deserialized game packets |
| Output encoding | N/A (no web UI) | Streaming gateway must sanitize any HTML for browser UI |
| Authentication | None | Must add OAuth 2.0 / OpenID Connect (Entra ID) |
| Authorization | None (console has full access) | RBAC: player vs admin vs service identity |
| Cryptography | None. Plaintext protocol. | TLS 1.3 external, mTLS internal, AEAD for game state |
| Error handling | `Sys_Error()` → exit, `longjmp` | Structured error handling, no stack unwinding hacks |
| Logging | `Con_Printf` to console | Structured JSON logs, no PII, audit trail |
| Data protection | No PII handling | Must handle player identity with GDPR compliance |

### 2. Cloud-Native Twelve-Factor Compliance

| Factor | Current State | Gap | Priority |
| --- | --- | --- | --- |
| I. Codebase | No VCS history in project | Must track in Git with branching strategy | P1 |
| II. Dependencies | Implicit OS deps, no manifest | CMakeLists.txt + Dockerfile for explicit deps | P0 |
| III. Config | Hardcoded in source, `quake.rc` | Environment variables or Azure App Configuration | P1 |
| IV. Backing services | None | Azure Files (assets), Key Vault (secrets), App Insights (telemetry) | P1 |
| V. Build/release/run | No separation | CI/CD pipeline: build image → tag → deploy | P0 |
| VI. Processes | Stateful singleton with mutable global state | Extract state to allow container restarts; checkpoint game state | P1 |
| VII. Port binding | OS-dependent socket binding | Listen on `$PORT` env var, HTTP/gRPC for APIs | P0 |
| VIII. Concurrency | Single-threaded, single-process | Horizontal scaling via multiple containers | P0 (architecture) |
| IX. Disposability | No graceful shutdown; `exit(1)` | SIGTERM handler, drain connections, save state | P1 |
| X. Dev/prod parity | No environments | Identical container images across dev/staging/prod | P1 |
| XI. Logs | Written to console buffer | Stdout/stderr structured JSON, collected by Azure Monitor | P1 |
| XII. Admin processes | Console commands embedded | Separate admin API or one-off container job | P2 |

### 3. Azure Well-Architected Compliance Gaps

**Reliability**:
- [ ] No health check endpoints
- [ ] No retry logic in network operations
- [ ] No graceful degradation
- [ ] No multi-region or zone redundancy
- [ ] No backup/restore for game state

**Security**:
- [ ] No identity provider integration
- [ ] No network segmentation
- [ ] No secret management
- [ ] No container image scanning
- [ ] No runtime threat detection
- [ ] No DDoS protection

**Cost Optimization**:
- [ ] No auto-scaling; fixed resource allocation
- [ ] No consumption-based billing model
- [ ] No resource tagging or cost attribution
- [ ] No idle resource detection

**Operational Excellence**:
- [ ] No IaC (manual deployment only)
- [ ] No CI/CD pipeline
- [ ] No monitoring dashboards
- [ ] No alerting rules
- [ ] No runbooks or incident procedures
- [ ] No SLA/SLO definitions

**Performance Efficiency**:
- [ ] No performance testing framework
- [ ] No auto-scaling based on load
- [ ] No CDN for static assets
- [ ] No connection pooling or caching layers

### 4. Container and Kubernetes Best Practices

| Practice | Status | Required Action |
| --- | --- | --- |
| Non-root container execution | Missing | Run as non-root user in Dockerfile |
| Read-only root filesystem | Missing | Mount writable dirs explicitly |
| Resource limits (CPU/memory) | Missing | Define in ACA container spec |
| Health probes (liveness/readiness) | Missing | HTTP endpoints on game worker |
| Graceful shutdown (SIGTERM) | Missing | Signal handler to save state and exit |
| Minimal base image | N/A (no image) | Use distroless or Alpine |
| Image scanning | N/A | Enable Defender for Containers on ACR |
| No secrets in image | N/A (no image) | Use Key Vault references |
| Immutable tags | N/A | Tag images with SHA digest |

### 5. DevOps Maturity Assessment

**Current Level: 0 — Manual / Ad-Hoc**

| Capability | Current | Target (Level 3) |
| --- | --- | --- |
| Version control | None (source dump) | Git with trunk-based development |
| Build automation | Manual Makefile | CMake + Docker multi-stage build |
| Test automation | None | Unit + integration + E2E tests |
| CI pipeline | None | GitHub Actions: build, test, scan, push |
| CD pipeline | None | GitHub Actions: deploy to ACA revisions |
| Infrastructure | Manual | Bicep IaC, `azd` for provisioning |
| Monitoring | None | Azure Monitor + App Insights |
| Incident management | None | Alerting rules + runbooks |
| Security scanning | None | Trivy/Defender in CI + runtime |
| Performance testing | None | k6 or Locust for load testing |

### 6. Accessibility and Browser Compliance

For the streaming browser client:
- [ ] WCAG 2.1 Level AA compliance for browser UI (not game content)
- [ ] Keyboard navigable menus and session management
- [ ] Screen reader support for non-game UI elements
- [ ] Responsive layout for different screen sizes
- [ ] Cross-browser support (Chrome, Firefox, Safari, Edge)
- [ ] Mobile browser input support (touch → game input mapping)

### 7. Data Protection and Privacy (GDPR/CCPA)

| Requirement | Status | Action |
| --- | --- | --- |
| Data inventory | No PII today | Document player identity data flows |
| Consent management | None | Implement consent for telemetry collection |
| Data minimization | N/A | Collect only necessary telemetry |
| Right to erasure | N/A | API to delete player data |
| Data retention policy | None | Define retention for sessions, telemetry, logs |
| Cross-border transfer | N/A | Deploy in user-proximate Azure regions |
| Breach notification | None | Incident response plan required |

## Priority Summary

| Priority | Gap Area | Impact |
| --- | --- | --- |
| P0 | Dependency management, build/release pipeline, port binding, horizontally scalable architecture | Cannot containerize without these |
| P1 | Authentication, config externalization, structured logging, graceful shutdown, Git workflow | Cannot go to production without these |
| P2 | Full WAF compliance, GDPR procedures, accessibility, advanced monitoring | Required for production maturity |
| P3 | Multi-region, advanced cost optimization, Level 3+ DevOps maturity | Future optimization |
