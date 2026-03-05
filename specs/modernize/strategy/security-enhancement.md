# Security Enhancement Strategy

## Security Architecture: Zero-Trust for Cloud Gaming

### Threat Model

**Attack Surfaces**:
1. **Browser → Streaming Gateway**: Public internet, WebRTC/WebSocket. Attacker can send malformed input, attempt session hijacking, or DoS.
2. **Browser → Session Manager API**: Public REST API. Attacker can attempt unauthorized session creation, enumeration, or resource exhaustion.
3. **Browser → Assets API**: Public HTTP. Attacker can attempt path traversal or cache poisoning.
4. **Internal services**: Container-to-container. Attacker with container escape could pivot.
5. **Game Worker**: Processes untrusted game data (maps, mods, demos). Buffer overflows in legacy C code.

### Actors
- **Anonymous user**: Unauthenticated browser visitor
- **Authenticated player**: Logged-in user with valid session
- **Malicious player**: Authenticated user sending crafted input
- **External attacker**: Targeting public endpoints
- **Compromised container**: Lateral movement within ACA environment

## Security Controls by Layer

### Layer 1: Edge / Perimeter

| Control | Implementation | Mitigates |
| --- | --- | --- |
| Azure Front Door | Global load balancer + TLS termination | Network exposure |
| Web Application Firewall (WAF) | Azure Front Door WAF v2, OWASP 3.2 ruleset | SQL injection, XSS, request smuggling |
| DDoS Protection | Azure DDoS Network Protection | Volumetric and protocol DDoS |
| Rate limiting | Front Door rate limit rules: 100 req/min per IP for APIs | Resource exhaustion |
| Geo-filtering | Optional: restrict to target regions | Reduce attack surface |

### Layer 2: Authentication and Authorization

| Control | Implementation | Mitigates |
| --- | --- | --- |
| Identity provider | Microsoft Entra ID (OAuth 2.0 / OIDC) | Unauthorized access |
| JWT validation | Session Manager validates Entra ID tokens | Token forgery |
| RBAC | Player, Admin roles in Entra ID app registration | Privilege escalation |
| Session binding | JWT `sub` claim tied to session; session-manager enforces ownership | Session hijacking |
| API authorization | Each service validates bearer token + scope | Broken access control |
| Service-to-service auth | Managed identity + workload identity federation | Service impersonation |

### Layer 3: Network Security

| Control | Implementation | Mitigates |
| --- | --- | --- |
| VNet integration | ACA environment in private VNet | Direct container access |
| Internal traffic encryption | mTLS between services (ACA managed certificates or Dapr) | Eavesdropping, MITM |
| Private endpoints | Azure Files, Key Vault, Cosmos DB via private endpoints | Data exfiltration |
| Network policies | ACA network rules: game workers cannot initiate external connections | Reverse shell, C2 |
| DNS security | Azure Private DNS zones | DNS spoofing |

### Layer 4: Container Security

| Control | Implementation | Mitigates |
| --- | --- | --- |
| Non-root execution | `USER quake` in Dockerfile | Container escape privilege |
| Read-only filesystem | `readOnlyRootFilesystem: true` + writable `/tmp` | Persistent compromise |
| Distroless / minimal base | `gcr.io/distroless/cc` or minimal Ubuntu | Unnecessary tools for attacker |
| Image scanning | Microsoft Defender for Containers on ACR | Known CVEs in base image |
| Seccomp profile | Default Docker seccomp (restrict dangerous syscalls) | Kernel exploits |
| No privileged mode | Never `--privileged` | Full host access |
| Image signing | Notation (Notary v2) for supply chain integrity | Tampered images |

### Layer 5: Application Security (Legacy Engine Fixes)

| Vulnerability | Fix | Priority |
| --- | --- | --- |
| `sprintf` buffer overflow (SEC-C01) | Replace all `sprintf` with `snprintf` | P0 |
| `svc_stufftext` RCE (SEC-C02) | Remove `svc_stufftext` from client command handling; whitelist allowed server commands | P0 |
| Path traversal in file loading (SEC-H01) | Sanitize all file paths; reject `..` components; restrict to `$QUAKE_BASEDIR` | P0 |
| Unbounded `strcpy`/`strcat` | Replace with bounds-checked alternatives | P1 |
| Self-modifying code in ASM routines | Remove all x86 ASM (builds with `-DNOASM`) | P1 |
| Integer overflow in memory allocator | Add overflow checks in `Hunk_Alloc`, `Z_Malloc` | P1 |
| Console command injection | Sanitize console input; remove `svc_stufftext` processing | P1 |

### Layer 6: Data Protection

| Control | Implementation | Mitigates |
| --- | --- | --- |
| Secrets management | Azure Key Vault; no secrets in env vars or images | Secret exposure |
| Managed identity | ACA system-assigned identity for Key Vault, Storage access | No stored credentials |
| Encryption at rest | Azure Storage encryption (default); Cosmos DB encryption | Data breach |
| Encryption in transit | TLS 1.3 external; mTLS internal; DTLS for WebRTC | Eavesdropping |
| PII minimization | Store only Entra ID `sub` claim; no player email in logs | GDPR compliance |
| Log sanitization | Strip PII from structured logs before emission | Data leakage via logs |

### Layer 7: Monitoring and Incident Response

| Control | Implementation | Mitigates |
| --- | --- | --- |
| Audit logging | All API calls logged with caller identity, action, timestamp | Non-repudiation |
| Security alerts | Microsoft Defender for Cloud alerts | Active threats |
| Anomaly detection | App Insights anomaly detection on session creation rate | Abuse patterns |
| Incident runbooks | Documented procedures for compromise, DoS, data breach | Slow incident response |
| Log retention | 90-day retention in Log Analytics | Forensic analysis |

## Implementation Phases

### Phase 0 (Foundation)
- Fix `sprintf` → `snprintf` in all source files
- Remove `svc_stufftext` processing
- Add path sanitization to file loading
- Remove x86 assembly code

### Phase 1 (Headless Worker)
- Non-root container, minimal base image
- Read-only filesystem, explicit writable mounts
- Health check endpoint (unauthenticated — internal only)
- SIGTERM graceful shutdown

### Phase 2 (Streaming Gateway)
- TLS 1.3 on all browser connections
- DTLS for WebRTC media
- Input validation on all browser-originated data
- Rate limiting on signaling WebSocket

### Phase 3 (Session Manager)
- Entra ID integration (OAuth 2.0 authorization code flow)
- JWT validation middleware
- RBAC enforcement
- Session ownership validation

### Phase 5 (Hardening)
- WAF deployment and rule tuning
- Penetration testing
- Defender for Cloud enablement
- Incident response runbook creation
- Security monitoring dashboards
