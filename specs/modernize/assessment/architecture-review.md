# Architecture Review

## Current Architecture: As-Is

WinQuake is a monolithic single-process, single-threaded game engine. All subsystems (rendering, physics, sound, network, input, console, QuakeC VM) are compiled into one binary and execute sequentially in a frame loop (`Host_Frame()`). Global mutable state is the primary inter-subsystem communication mechanism.

```
┌──────────────────────────────────────────────┐
│            WinQuake Monolith                  │
│                                              │
│  ┌─────┐ ┌──────┐ ┌─────┐ ┌───────┐        │
│  │Render│ │Sound │ │Input│ │Network│        │
│  │r_*.c │ │snd_*.│ │in_*.│ │net_*. │        │
│  └──┬───┘ └──┬───┘ └──┬──┘ └──┬────┘        │
│     │        │        │       │              │
│  ┌──┴────────┴────────┴───────┴──┐           │
│  │     Host_Frame() Main Loop     │           │
│  │      host.c (single thread)    │           │
│  └──┬─────────────────────┬──────┘           │
│     │                     │                  │
│  ┌──┴──────┐       ┌─────┴────┐             │
│  │ Server  │◄─────►│  Client  │             │
│  │sv_*.c   │loopback│cl_*.c   │             │
│  │pr_*.c   │       │          │             │
│  └─────────┘       └──────────┘             │
│                                              │
│  Platform: Win32 │ Linux │ DOS │ Solaris     │
└──────────────────────────────────────────────┘
      ▼                     ▲
  OS Display            OS Input
  OS Sound              OS Network
```

## Target Architecture: Cloud-Native Streaming

```
                    ┌──────────────────┐
                    │   Azure Front    │
                    │    Door + WAF    │
                    └────────┬─────────┘
                             │ HTTPS/WSS
         ┌───────────────────┼───────────────────┐
         ▼                   ▼                   ▼
┌─────────────────┐ ┌───────────────┐ ┌──────────────────┐
│  Session Manager │ │  Assets API   │ │  Telemetry API   │
│  (Container App) │ │ (Container App│ │  (Container App)  │
│                  │ │  + Azure CDN) │ │                   │
│  - Create/stop   │ │               │ │  - Ingest metrics │
│    game workers  │ │  - Serve PAK  │ │  - Player events  │
│  - Capacity mgmt │ │    contents   │ │  - Session logs   │
│  - Auth (Entra)  │ │  - Mod assets │ │  → App Insights   │
│  - Session state │ │  - Thumbnails │ │  → Log Analytics   │
└────────┬─────────┘ └───────────────┘ └──────────────────┘
         │ manages
         ▼
┌─────────────────────────────────────────────────┐
│           Streaming Gateway                      │
│           (Container App)                        │
│                                                  │
│  Browser ◄──WebRTC/WSS──► Gateway ◄──► Worker   │
│                                                  │
│  - WebRTC SFU or WebSocket binary stream         │
│  - H.264/VP9 video mux                          │
│  - Opus audio mux                               │
│  - Input demux (keyboard/mouse → worker)        │
│  - Session affinity (sticky to worker)           │
└─────────────────┬───────────────────────────────┘
                  │ internal gRPC / shared memory
                  ▼
┌─────────────────────────────────────────────────┐
│           Game Worker Pool                       │
│           (Container Apps, auto-scaled)           │
│                                                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │ Worker 1 │ │ Worker 2 │ │ Worker N │        │
│  │          │ │          │ │          │        │
│  │ Quake    │ │ Quake    │ │ Quake    │        │
│  │ Headless │ │ Headless │ │ Headless │        │
│  │ Server + │ │ Server + │ │ Server + │        │
│  │ Renderer │ │ Renderer │ │ Renderer │        │
│  │ + Encode │ │ + Encode │ │ + Encode │        │
│  └──────────┘ └──────────┘ └──────────┘        │
│                                                  │
│  Assets: Azure Files (shared, read-only mount)   │
└─────────────────────────────────────────────────┘

Supporting Infrastructure:
┌─────────────────┐ ┌──────────────┐ ┌──────────────┐
│ Azure Container │ │ Azure Key    │ │ Azure Monitor│
│ Registry (ACR)  │ │ Vault        │ │ + App        │
│                 │ │              │ │ Insights     │
│ Game worker     │ │ Secrets,     │ │              │
│ images          │ │ tokens       │ │ Metrics,     │
│                 │ │              │ │ logs, traces │
└─────────────────┘ └──────────────┘ └──────────────┘
```

## Well-Architected Framework Assessment

### Pillar 1: Reliability — SCORE: 1/5 (Current) → 4/5 (Target)

**Current Gaps**:
- Single process = single point of failure. `Sys_Error()` terminates everything.
- `setjmp`/`longjmp` error recovery is brittle (skips cleanup).
- No health checks, no restart automation.
- Network disconnects lose all game state.

**Target Controls**:
- Container orchestration auto-restarts failed workers
- Session-manager maintains desired capacity with min replicas
- Health check endpoints (`/healthz`, `/readyz`) for liveness/readiness probes
- Game state checkpointing for session recovery
- Multi-zone deployment for Azure region resilience

### Pillar 2: Security — SCORE: 0/5 (Current) → 4/5 (Target)

**Current Gaps** (see security-audit.md):
- No authentication, no encryption, buffer overflows, `svc_stufftext` RCE, path traversal

**Target Controls**:
- Microsoft Entra ID for user auth; JWT tokens for session binding
- TLS 1.3 on all external traffic; mTLS between internal services
- Game workers in private VNet, no direct internet access
- WAF on public endpoints (Azure Front Door)
- Non-root distroless containers, seccomp profiles
- Secrets in Azure Key Vault

### Pillar 3: Cost Optimization — SCORE: N/A (Current) → 3/5 (Target)

**Target Controls**:
- Azure Container Apps consumption plan: pay per active session
- Auto-scaling: scale workers from 0 → N based on demand
- Pre-warmed pool minimizes cold starts while controlling idle cost
- Shared asset volume (Azure Files) avoids per-container storage duplication
- Software encoding avoids GPU SKU premium (unless density requires it)
- Spot instances for non-production environments

### Pillar 4: Performance Efficiency — SCORE: 3/5 (Context-Relative) → 4/5 (Target)

**Current**: Highly optimized for 1996 CPUs but irrelevant optimizations for cloud.
**Target Controls**:
- Mesa LLVMpipe or GPU for efficient headless rendering
- FFmpeg hardware-accelerated encoding where available
- WebRTC with adaptive bitrate for network efficiency
- Co-located gateway + workers (same ACA environment) for minimal latency
- Asset caching at CDN layer (Azure CDN) for assets-api

### Pillar 5: Operational Excellence — SCORE: 0/5 (Current) → 4/5 (Target)

**Current Gaps**:
- No monitoring, logging, alerting, or observability
- No automated deployment
- No infrastructure as code
- No documentation of operational procedures

**Target Controls**:
- Structured logging (JSON) to Azure Monitor / Log Analytics
- Distributed tracing via OpenTelemetry
- Azure Application Insights for metrics and dashboards
- Bicep IaC for all Azure resources
- GitHub Actions CI/CD pipeline
- Automated canary deployments via ACA revisions

## Architecture Migration Strategy

### Approach: Strangler Fig + Internal Decomposition

1. **Phase 1 — Headless Game Worker**: Modify WinQuake to run headless (no display, no sound hardware). Replace `VID_Update()` with framebuffer export. Replace `snd_dma.c` with buffer capture. Result: the engine runs in a container and exposes frames + audio buffers.

2. **Phase 2 — Streaming Gateway**: Build new service that connects to game worker, encodes video/audio, streams to browser via WebRTC. Receives browser input and injects into worker.

3. **Phase 3 — Session Manager**: Build orchestration service that creates/destroys game workers on demand, manages capacity, handles authentication.

4. **Phase 4 — Supporting Services**: Assets-API (serve game content to browser for loading screens, thumbnails), Telemetry-API (collect metrics and events).

This is a strangler fig approach: the legacy monolith becomes the inner "game worker" component, wrapped by new cloud-native services. The monolith itself is minimally modified—headless mode + capture APIs—while all new capabilities (streaming, auth, orchestration) are new services.
