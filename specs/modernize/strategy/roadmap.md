# Modernization Roadmap

## Overview

Transform WinQuake from a 1996 monolithic desktop game engine into a cloud-native streaming platform on Azure Container Apps. The engine becomes a headless game worker; new microservices handle streaming, orchestration, assets, and telemetry.

**Approach**: Strangler Fig — wrap the minimally-modified legacy engine in new cloud-native services rather than rewriting it.

## Phase 0: Foundation (Weeks 1–3)

**Goal**: Modern build system, version control, and basic CI.

| Task | Description | Deliverable |
| --- | --- | --- |
| 0.1 | Initialize Git repository with `.gitignore` | Tracked codebase |
| 0.2 | Create CMakeLists.txt replacing MSVC 6.0 Makefile | Cross-platform build |
| 0.3 | Strip x86 assembly; use C fallbacks (`NOASM` path) | Portable codebase |
| 0.4 | Fix all `sprintf` → `snprintf` and buffer safety | Eliminate buffer overflows |
| 0.5 | Set up GitHub Actions: build on Linux (gcc/clang) | Green CI pipeline |
| 0.6 | Create Dockerfile (multi-stage: build + runtime) | Containerized binary |
| 0.7 | Create Bicep IaC for ACR + base ACA environment | Infrastructure scaffold |

**Exit Criteria**: `docker build` produces a runnable container. CI builds pass.

## Phase 1: Headless Game Worker (Weeks 4–8)

**Goal**: WinQuake runs headless in a container, exports framebuffer and audio.

| Task | Description | Deliverable |
| --- | --- | --- |
| 1.1 | Replace `vid_*.c` with headless video driver (Mesa LLVMpipe) | No display dependency |
| 1.2 | Replace `snd_dma.c` with audio capture driver (ring buffer) | No sound hardware dependency |
| 1.3 | Replace `in_*.c` with programmatic input injection API | No HID dependency |
| 1.4 | Implement framebuffer capture API (FBO readback → shared buffer) | Frame export interface |
| 1.5 | Implement audio capture API (ring buffer → PCM export) | Audio export interface |
| 1.6 | Add HTTP health check endpoint (`/healthz`) | Container orchestration ready |
| 1.7 | Add SIGTERM graceful shutdown handler | Clean container lifecycle |
| 1.8 | Externalize config via environment variables | Twelve-factor config |
| 1.9 | Replace `Con_Printf` with structured JSON logging to stdout | Observable output |
| 1.10 | Containerize headless worker image | Deployable artifact |

**Exit Criteria**: Worker container starts, runs a game loop, produces frames and audio buffers accessible via API, responds to `/healthz`, shuts down cleanly on SIGTERM.

## Phase 2: Streaming Gateway (Weeks 9–14)

**Goal**: Browser receives video/audio stream and sends input.

| Task | Description | Deliverable |
| --- | --- | --- |
| 2.1 | Build streaming gateway service (Go or Rust) | New service |
| 2.2 | Integrate FFmpeg for H.264/VP9 video encoding | Encoded video stream |
| 2.3 | Integrate libopus for audio encoding | Encoded audio stream |
| 2.4 | Implement WebRTC signaling and media transport | Browser-compatible stream |
| 2.5 | Implement WebSocket fallback for constrained networks | Broad compatibility |
| 2.6 | Implement input demux (browser events → worker input API) | Player control |
| 2.7 | Implement adaptive bitrate and resolution scaling | Network resilience |
| 2.8 | Build browser client (HTML5 + JS) | Playable in browser |
| 2.9 | Add TLS termination (Azure Front Door integration) | Encrypted external traffic |
| 2.10 | Containerize gateway and deploy to ACA | Running service |

**Exit Criteria**: Browser connects, sees game video, hears audio, can play with keyboard/mouse. Latency < 100ms input-to-display.

## Phase 3: Session Manager (Weeks 15–18)

**Goal**: Automated game session lifecycle management.

| Task | Description | Deliverable |
| --- | --- | --- |
| 3.1 | Build session manager service (Go or C#) | New service |
| 3.2 | Integrate Microsoft Entra ID for authentication | Authenticated sessions |
| 3.3 | Implement session create/join/leave/destroy APIs | Session lifecycle |
| 3.4 | Implement game worker provisioning (ACA scaling) | Dynamic capacity |
| 3.5 | Implement session-to-worker affinity routing | Sticky sessions |
| 3.6 | Add capacity management and limits | Resource governance |
| 3.7 | Store session state in Azure Cosmos DB or Redis | Persistent session data |
| 3.8 | Containerize and deploy | Running service |

**Exit Criteria**: Users authenticate, sessions auto-create workers, capacity scales with demand.

## Phase 4: Supporting Services (Weeks 19–22)

**Goal**: Assets serving and telemetry collection.

| Task | Description | Deliverable |
| --- | --- | --- |
| 4.1 | Build assets-api service | Game asset serving |
| 4.2 | Extract PAK file contents to Azure Blob Storage | CDN-ready assets |
| 4.3 | Integrate Azure CDN for asset delivery | Fast global assets |
| 4.4 | Build telemetry-api service | Metrics ingestion |
| 4.5 | Integrate Azure Application Insights | Observability |
| 4.6 | Implement OpenTelemetry distributed tracing across all services | End-to-end traces |
| 4.7 | Build monitoring dashboards (Azure Monitor workbooks) | Operational visibility |
| 4.8 | Set up alerting rules for SLO breaches | Proactive operations |

**Exit Criteria**: Assets served via CDN, telemetry flowing to App Insights, dashboards operational.

## Phase 5: Hardening and Production Readiness (Weeks 23–26)

**Goal**: Security, compliance, and operational maturity.

| Task | Description | Deliverable |
| --- | --- | --- |
| 5.1 | Security penetration testing | Verified security posture |
| 5.2 | Performance load testing (target: 100 concurrent sessions) | Verified scalability |
| 5.3 | Implement WAF rules on Azure Front Door | DDoS/attack protection |
| 5.4 | Enable Defender for Containers on ACR | Image vulnerability scanning |
| 5.5 | Create runbooks for incident response | Operational procedures |
| 5.6 | Implement canary deployment via ACA revisions | Safe deployments |
| 5.7 | Set up cost alerting and budgets | Cost governance |
| 5.8 | Final compliance review (GDPR, accessibility) | Compliance sign-off |

**Exit Criteria**: Production-ready system with security, monitoring, and operational procedures.

## Milestone Summary

| Milestone | Week | Key Outcome |
| --- | --- | --- |
| M0 | 3 | Containerized build, CI green |
| M1 | 8 | Headless game worker running in container |
| M2 | 14 | Browser can play Quake via streaming |
| M3 | 18 | Authenticated sessions with auto-scaling |
| M4 | 22 | Full platform with assets + telemetry |
| M5 | 26 | Production-ready, security-hardened |

## Dependencies Between Phases

```
Phase 0 ──► Phase 1 ──► Phase 2 ──► Phase 5
                │              │
                └──► Phase 3 ──┤
                               │
                     Phase 4 ──┘
```

- Phase 0 is prerequisite for all others
- Phase 1 is prerequisite for Phase 2; Phase 2 is prerequisite for Phase 3
- Phase 4 can proceed in parallel with Phase 3 after Phase 2 starts
- Phase 5 requires all other phases complete
