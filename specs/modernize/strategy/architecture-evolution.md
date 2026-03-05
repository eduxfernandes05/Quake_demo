# Architecture Evolution Plan

## From Monolith to Cloud-Native Microservices

### Current State: Monolithic Desktop Application
A single binary containing all game engine subsystems, tightly coupled through global state, running as a desktop application with direct hardware access.

### Target State: 5-Service Cloud Streaming Platform

```
┌─────────────────────────────────────────────────────────────┐
│                    Azure Container Apps Environment           │
│                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ Session Mgr  │  │  Assets API  │  │ Telemetry API│       │
│  │ C# / Go      │  │ Go / Node    │  │ Go / C#      │       │
│  │              │  │              │  │              │       │
│  │ POST /session│  │ GET /assets/ │  │ POST /events │       │
│  │ GET /session │  │ GET /maps/   │  │ GET /metrics │       │
│  │ DEL /session │  │ GET /mods/   │  │              │       │
│  └──────┬───────┘  └──────────────┘  └──────────────┘       │
│         │ creates/destroys                                    │
│         ▼                                                     │
│  ┌──────────────────────────────────────────────────┐        │
│  │           Streaming Gateway                       │        │
│  │           Go / Rust                               │        │
│  │                                                   │        │
│  │  WebRTC ◄──► Encode ◄──► Worker IPC              │        │
│  │  (browser)   (FFmpeg)    (shared mem / gRPC)      │        │
│  └──────────────────┬───────────────────────────────┘        │
│                     │ co-located                              │
│  ┌──────────────────▼───────────────────────────────┐        │
│  │           Game Worker (Headless Quake)             │        │
│  │           C11                                      │        │
│  │                                                   │        │
│  │  Mesa LLVMpipe → FBO → framebuffer export         │        │
│  │  Audio mixer → ring buffer → PCM export           │        │
│  │  Input injection API ← gateway                    │        │
│  │  Health: /healthz                                 │        │
│  └───────────────────────────────────────────────────┘        │
│                                                               │
│  Shared: Azure Files (game assets, read-only)                │
└─────────────────────────────────────────────────────────────┘
```

## Service Definitions

### 1. Game Worker Service

**Responsibility**: Run the Quake engine headless, producing video frames and audio samples, accepting input commands.

**Technology**: Modified WinQuake C codebase compiled for Linux, using Mesa LLVMpipe for software OpenGL rendering.

**Interfaces**:
| Interface | Type | Description |
| --- | --- | --- |
| Frame export | Shared memory or Unix socket | RGBA framebuffer at 720p, 30 fps |
| Audio export | Shared memory or Unix socket | PCM 16-bit, 11025 Hz (Quake native) or upsampled to 48 kHz |
| Input injection | Function call / Unix socket | Key events, mouse deltas, button state |
| Health check | HTTP GET `/healthz` | Returns 200 when game loop is running |
| Config | Environment variables | `QUAKE_MAP`, `QUAKE_SKILL`, `QUAKE_BASEDIR`, etc. |

**Scaling**: One container per game session. Scaled to 0 when no sessions active.

**Resources**: 2 vCPU, 512 MB RAM per instance (software rendering).

### 2. Streaming Gateway Service

**Responsibility**: Encode game worker output to video/audio streams, transport to browser, receive and forward player input.

**Technology**: Go or Rust service using FFmpeg for encoding, WebRTC for transport.

**Interfaces**:
| Interface | Direction | Description |
| --- | --- | --- |
| Worker frame input | Gateway ← Worker | Raw RGBA frames via shared memory |
| Worker audio input | Gateway ← Worker | Raw PCM audio via shared memory |
| Worker input output | Gateway → Worker | Keyboard/mouse events |
| Browser video | Gateway → Browser | H.264 or VP9 RTP stream (WebRTC) |
| Browser audio | Gateway → Browser | Opus RTP stream (WebRTC) |
| Browser input | Gateway ← Browser | WebRTC data channel or WebSocket |
| Signaling | Bidirectional | WebSocket for WebRTC session setup |

**Scaling**: Co-located with game worker (1:1 pairing via ACA container groups or sidecar pattern).

**Resources**: 2 vCPU, 256 MB RAM per instance (video encoding is CPU-intensive).

### 3. Session Manager Service

**Responsibility**: Manage game session lifecycle — creation, routing, capacity, authentication.

**Technology**: C# (.NET 8) or Go.

**API**:
```
POST   /api/sessions              Create new session (auth required)
GET    /api/sessions/{id}         Get session status + connection info
DELETE /api/sessions/{id}         End session
GET    /api/sessions              List user's sessions
GET    /api/capacity              Current capacity and availability
```

**Integrations**:
- Microsoft Entra ID for OAuth 2.0 / OpenID Connect authentication
- Azure Container Apps Management API for scaling game workers
- Azure Cosmos DB or Redis for session state
- Telemetry API for session events

**Scaling**: 2+ replicas for HA. Stateless (state in external store).

### 4. Assets API Service

**Responsibility**: Serve game assets (maps, models, textures, sounds) extracted from PAK files.

**Technology**: Go or Node.js.

**API**:
```
GET /api/assets/maps                   List available maps
GET /api/assets/maps/{name}/thumbnail  Map preview image
GET /api/assets/paks/{name}            Download PAK file
GET /api/assets/mods                   List available mods
GET /api/assets/health                 Health check
```

**Storage**: Azure Blob Storage (extracted PAK contents) + Azure CDN for caching.

**Scaling**: Stateless, auto-scales on HTTP request volume. CDN handles most traffic.

### 5. Telemetry API Service

**Responsibility**: Collect metrics, player events, session logs. Forward to Azure Application Insights.

**Technology**: Go or C# (.NET 8).

**API**:
```
POST /api/telemetry/events       Ingest player/session events
POST /api/telemetry/metrics      Ingest custom metrics
GET  /api/telemetry/health       Health check
```

**Integrations**:
- Azure Application Insights (metrics, traces)
- Azure Log Analytics (structured logs)
- OpenTelemetry SDK for distributed tracing

**Scaling**: Stateless, auto-scales. Async processing via buffer/queue.

## Inter-Service Communication

| From | To | Protocol | Pattern |
| --- | --- | --- | --- |
| Browser | Streaming Gateway | WebRTC / WebSocket (WSS) | Bidirectional streaming |
| Browser | Session Manager | HTTPS REST | Request-response |
| Browser | Assets API | HTTPS REST | Request-response (CDN cached) |
| Session Manager | Game Worker + Gateway | ACA Management API | Provisioning (create/scale containers) |
| Streaming Gateway | Game Worker | Shared memory / Unix socket | Streaming (co-located) |
| All services | Telemetry API | HTTPS REST / OpenTelemetry gRPC | Fire-and-forget events |
| All services | Azure Key Vault | HTTPS | Secret retrieval (managed identity) |

## Data Flow: Player Session Lifecycle

```
1. Browser → Session Manager: POST /api/sessions (JWT token)
2. Session Manager → ACA: Create game-worker + streaming-gateway pair
3. Session Manager → Browser: { sessionId, websocketUrl }
4. Browser → Streaming Gateway: WebSocket signaling → WebRTC negotiation
5. Game Worker: starts game loop, renders frames, mixes audio
6. Game Worker → Gateway: RGBA frames + PCM audio (shared memory)
7. Gateway: encode H.264 + Opus → WebRTC RTP
8. Gateway → Browser: video + audio stream
9. Browser → Gateway: keyboard/mouse events (data channel)
10. Gateway → Game Worker: inject input events
11. (Loop 5–10 at 30 fps)
12. Browser → Session Manager: DELETE /api/sessions/{id}
13. Session Manager → ACA: destroy worker + gateway containers
```

## Architectural Patterns

### Sidecar / Container Group
Game Worker and Streaming Gateway are co-located in the same ACA container group (pod). They share:
- Network namespace (localhost communication)
- Shared memory volume for frame/audio transfer
- Lifecycle (created and destroyed together)

### Strangler Fig
The legacy Quake engine (game worker) is not rewritten — it is wrapped. New services handle all modern concerns (auth, streaming, orchestration). The engine only needs minimal modifications (headless drivers, capture APIs, health endpoint).

### Backend for Frontend (BFF)
The Streaming Gateway acts as a BFF for the browser client — it handles protocol translation (game engine formats → browser-compatible WebRTC streams) and input translation (browser events → game engine commands).

### Event-Driven Telemetry
All services emit events asynchronously to the Telemetry API. No synchronous coupling for observability.

## Migration Boundaries

### What Changes in WinQuake
- Video driver: new `vid_headless.c` (Mesa LLVMpipe + FBO)
- Sound driver: new `snd_capture.c` (ring buffer)
- Input driver: new `in_inject.c` (queue-based injection)
- System layer: new `sys_container.c` (SIGTERM, env vars, JSON logging, health endpoint)
- Build: CMakeLists.txt replacing Makefile
- Security: `sprintf` → `snprintf` everywhere

### What Does NOT Change in WinQuake
- Game logic (`sv_*.c`, `pr_*.c`, QuakeC VM)
- Physics and movement (`sv_phys.c`, `sv_move.c`)
- BSP rendering pipeline (`r_*.c` — used via GL path)
- Audio mixer (`snd_mix.c`, `snd_dma.c` core logic)
- Console and command system (`cmd.c`, `cvar.c`, `console.c`)
- Network protocol (used only for multiplayer between workers)
- File loading / PAK system (`com_*.c`, `wad.c`)
