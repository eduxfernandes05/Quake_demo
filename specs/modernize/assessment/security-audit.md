# Security Audit

## Overview

WinQuake has **no security architecture**. It was designed for trusted LAN environments in 1996. Transforming it into a cloud-hosted, internet-facing service requires addressing every layer—network, memory, filesystem, authentication, and data protection.

## Severity Ratings

### CRITICAL — Must Fix Before Cloud Deployment

#### SEC-C01: Buffer Overflow via `sprintf`/`vsprintf`
- **Locations**: `Con_Printf`, `Con_DPrintf`, `Con_DebugLog`, `Host_EndGame`, `Sys_Error`, `va()`, and ~40+ other call sites
- **Impact**: Remote code execution if attacker controls format input via network messages
- **Example**: `Con_Printf` uses `char msg[4096]` with `vsprintf(msg, fmt, argptr)` — no bounds limit
- **CVSS**: 9.8 (Network / Low complexity / No auth required)
- **Remediation**: Replace all `sprintf` → `snprintf`, all `vsprintf` → `vsnprintf`

#### SEC-C02: `svc_stufftext` Remote Command Execution
- **Location**: `cl_parse.c` — server sends arbitrary console commands to client
- **Impact**: Malicious server can execute any engine command on connecting clients
- **CVSS**: 9.1 (Network / Low complexity / Requires connection)
- **Remediation**: In cloud model, the game worker IS the server—eliminate `svc_stufftext` from streaming gateway protocol entirely. Whitelist allowed commands if any client→server command injection is needed.

#### SEC-C03: No Authentication
- **Location**: `net_dgrm.c` connection handshake — only checks game name ("QUAKE") and protocol version
- **Impact**: Anyone can connect to a game worker; session hijacking trivial
- **CVSS**: 9.1 (in cloud-exposed environment)
- **Remediation**: All connections must go through session-manager with token-based auth (JWT/OAuth2). Game workers must never be directly reachable from internet.

#### SEC-C04: Plaintext Network Protocol
- **Location**: All of `net_*.c`, `protocol.h`
- **Impact**: All game state, player input, chat transmitted in clear; man-in-the-middle trivial
- **CVSS**: 7.5 (cloud environment with shared networking)
- **Remediation**: Transport replaced by TLS-encrypted WebSocket (streaming gateway) and internal mTLS between services.

### HIGH — Fix During Modernization

#### SEC-H01: Path Traversal in File System
- **Location**: `common.c` (`COM_FindFile`, `COM_LoadFile`) — raw string concatenation for file paths
- **Impact**: Crafted file name with `../` could access files outside game directory
- **CVSS**: 7.5 (if assets-api serves user-uploaded content)
- **Remediation**: Sanitize all file paths; canonicalize and verify within allowed base directory. Assets-api must validate all asset paths.

#### SEC-H02: No Rate Limiting
- **Location**: `net_dgrm.c` — accepts unlimited connection requests
- **Impact**: Denial of service via connection flooding
- **CVSS**: 7.5
- **Remediation**: Rate limiting at streaming gateway (Azure API Management or Container Apps ingress). Internal services use ACA scaling rules.

#### SEC-H03: Self-Modifying Code
- **Location**: `Sys_MakeCodeWriteable()` in sys.h — marks code pages writable for x86 ASM
- **Impact**: Disables W^X/DEP protection; enables code injection payloads
- **CVSS**: 7.0 (local exploitation)
- **Remediation**: Remove x86 assembly entirely → remove `Sys_MakeCodeWriteable()`. Container runs with `seccomp` profile and `no-new-privileges`.

#### SEC-H04: World-Readable Log Files
- **Location**: `Con_DebugLog` — creates files with `O_CREAT` mode `0666`
- **Impact**: Sensitive data in logs accessible to any user in container
- **CVSS**: 5.3
- **Remediation**: Logs go to stdout (container best practice). No file-based logging. File permissions `0600` if any files needed.

### MEDIUM — Address in Architecture Phase

#### SEC-M01: Custom Memory Allocator Without Bounds Checking
- **Location**: `zone.c` — Hunk, Zone, Cache allocators
- **Impact**: Heap corruption possible; sentinel checks are minimal
- **Remediation**: Replace or augment with AddressSanitizer in debug builds; container memory limits via cgroups.

#### SEC-M02: No Input Validation on Console Commands
- **Location**: `cmd.c` (`Cmd_ExecuteString`) — executes any command string
- **Impact**: If any user input reaches command execution, arbitrary engine commands run
- **Remediation**: In cloud model, console commands come only from session-manager admin API, never from browser client.

#### SEC-M03: `setjmp`/`longjmp` Error Handling
- **Location**: `host.c` — `host_abortserver` used for error recovery
- **Impact**: Stack unwinding bypasses cleanup; resource leaks
- **Remediation**: Replace with structured error propagation; container restart as crash recovery.

## Cloud-Specific Security Requirements

### New Attack Surfaces

| Surface | Threat | Control |
|---------|--------|---------|
| **Streaming gateway (public)** | DDoS, session hijack, input injection | WAF, rate limiting, JWT auth, CORS |
| **Session-manager API (public)** | Unauthorized worker creation, resource exhaustion | OAuth2, RBAC, quota limits |
| **Assets-API (public)** | Path traversal, malicious upload, bandwidth abuse | CDN, signed URLs, content validation |
| **Telemetry-API (internal)** | Data exfiltration, log injection | Internal VNet only, structured logging schema |
| **Game worker (internal)** | Container escape, resource abuse | No direct internet, resource limits, seccomp |

### Required Security Controls

| Control | Implementation |
|---------|----------------|
| **Authentication** | Microsoft Entra ID / OAuth2 for session-manager and admin APIs |
| **Authorization** | RBAC: admin (manage workers), player (connect to assigned worker) |
| **Transport encryption** | TLS 1.3 on all external endpoints; mTLS between internal services |
| **Network segmentation** | Game workers and telemetry in private VNet; only gateway is public |
| **Container security** | Minimal base image (distroless), non-root user, read-only filesystem |
| **Secrets management** | Azure Key Vault for connection strings, tokens |
| **WAF** | Azure Front Door WAF on streaming gateway |
| **Monitoring** | Azure Monitor + Microsoft Defender for Cloud on all containers |
| **Image scanning** | Microsoft Defender for container registries |

## Compliance Gaps

| Standard | Gap | Remediation |
|----------|-----|-------------|
| OWASP Top 10 — Injection (A03) | `sprintf`, `stufftext`, path traversal | Buffer-safe functions, input whitelisting, path sanitization |
| OWASP Top 10 — Broken Access Control (A01) | No auth whatsoever | OAuth2 + RBAC on all APIs |
| OWASP Top 10 — Cryptographic Failures (A02) | No encryption anywhere | TLS 1.3 + mTLS + data-at-rest encryption |
| OWASP Top 10 — Security Misconfiguration (A05) | `setuid root`, `0666` file modes | Non-root containers, strict permissions |
| OWASP Top 10 — SSRF (A10) | File loading from user-supplied paths | Validate and whitelist all file paths |
| CIS Container Benchmarks | No container hardening | Distroless image, non-root, seccomp, read-only rootfs |
