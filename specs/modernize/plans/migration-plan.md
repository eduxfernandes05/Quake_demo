# Migration Plan

## Step-by-Step: Monolithic Desktop Game → Cloud Streaming Platform

### Pre-Migration Checklist

- [ ] All reverse engineering documentation reviewed and validated
- [ ] Modernization assessment documents approved
- [ ] Azure subscription provisioned with required resource providers
- [ ] GitHub repository created and team access configured
- [ ] Development environment (Linux, Docker, CMake) set up for all engineers

---

## Step 1: Repository Initialization

**Input**: WinQuake source files  
**Output**: Clean Git repository with CI

1. Initialize Git repository
2. Create `.gitignore`:
   ```
   build/
   *.o
   *.exe
   *.Zone.Identifier
   id1/
   *.pak
   ```
3. Import WinQuake source (exclude `.Zone.Identifier` files, binaries)
4. Create initial `CMakeLists.txt` that builds on Linux with gcc
5. Verify: `cmake -B build && cmake --build build` produces a binary
6. Set up GitHub Actions workflow: build on push
7. Verify: CI is green on `main`

**Rollback**: Delete repository, restart.

---

## Step 2: Code Safety Hardening

**Input**: Building codebase  
**Output**: Buffer-safe codebase without ASM

1. Run `grep -rn 'sprintf\b'` to inventory all sprintf calls
2. Replace each `sprintf(buf, fmt, ...)` with `snprintf(buf, sizeof(buf), fmt, ...)`
3. Replace `strcpy`/`strcat` with bounds-checked alternatives
4. Build with `-DNOASM` to exclude x86 assembly
5. Remove all `.s` assembly files from CMakeLists.txt
6. Remove `svc_stufftext` command processing from `cl_parse.c`
7. Add path sanitization to file loading in `common.c` (`COM_FindFile`, `COM_LoadFile`)
8. Build with AddressSanitizer: `cmake -B build -DCMAKE_C_FLAGS="-fsanitize=address"`
9. Run: start map, move around, verify no ASan violations
10. Add `cppcheck` and `clang-tidy` to CI

**Rollback**: Revert commits. Core engine logic is unchanged.

---

## Step 3: Headless Video Driver

**Input**: Safe codebase  
**Output**: Quake renders to framebuffer without display

1. Create `vid_headless.c` implementing `viddef_t` interface
2. Initialize Mesa EGL + LLVMpipe context (off-screen)
3. Create FBO for render target
4. `VID_Update()` reads FBO pixels into shared buffer (no display blit)
5. Export `VID_CaptureFrame(uint8_t **rgba, int *width, int *height)`
6. Add `-DHEADLESS=ON` CMake option that selects `vid_headless.c`
7. Build and test: engine starts, `Host_Frame()` loop runs, frames captured
8. Verify: write first captured frame to PNG for visual validation

**Rollback**: CMake option disabled; original vid drivers still present.

---

## Step 4: Headless Audio Driver

**Input**: Headless video working  
**Output**: Audio mixed to capturable ring buffer

1. Create `snd_capture.c` based on `snd_null.c` structure
2. Implement fake DMA buffer: allocate ring buffer that `snd_dma.c` mixer writes to
3. Set `shm->buffer` to ring buffer; `shm->speed = 11025`; `shm->samplebits = 16`
4. Implement `SNDDMA_GetDMAPos()` returning advancing position
5. Export `SND_CaptureAudio(int16_t **pcm, int *samples, int *rate)`
6. Add to CMake under `-DHEADLESS=ON`
7. Verify: load map, trigger sounds, capture buffer contains non-zero PCM data

**Rollback**: Falls back to `snd_null.c` (silent).

---

## Step 5: Input Injection Driver

**Input**: Headless video + audio  
**Output**: Game accepts programmatic input

1. Create `in_inject.c` implementing input driver interface
2. Implement key event queue: `IN_InjectKeyEvent(int key, qboolean down)`
3. Implement mouse event queue: `IN_InjectMouseEvent(int dx, int dy, int buttons)`
4. `IN_Move()` dequeues events and updates `cl.viewangles`, key states
5. Add to CMake under `-DHEADLESS=ON`
6. Verify: inject "forward" key → player moves forward in captured frames

**Rollback**: Input injection is additive; remove from CMake.

---

## Step 6: Container System Layer

**Input**: Headless engine with capture APIs  
**Output**: Container-ready system layer

1. Create `sys_container.c` replacing `sys_linux.c` for headless builds
2. Implement SIGTERM handler: set `host_shutdown` flag, `Host_Shutdown()` on next frame
3. Read config from env vars: `QUAKE_BASEDIR`, `QUAKE_MAP`, `QUAKE_SKILL`
4. Replace `Con_Printf` output path with structured JSON to stdout
5. Embed minimal HTTP server (or use sidecar) for `/healthz` endpoint
6. Implement: `/healthz` returns 200 when `Host_Frame()` loop is running
7. Create `Dockerfile`:
   ```dockerfile
   FROM ubuntu:24.04 AS builder
   RUN apt-get update && apt-get install -y build-essential cmake libgl-dev libegl-dev libgbm-dev
   COPY . /src
   WORKDIR /src
   RUN cmake -B build -DHEADLESS=ON -DNOASM=ON && cmake --build build
   
   FROM ubuntu:24.04
   RUN apt-get update && apt-get install -y libgl1-mesa-dri libegl1 libgbm1 && rm -rf /var/lib/apt/lists/*
   RUN useradd -r -s /usr/sbin/nologin quake
   COPY --from=builder /src/build/quake-worker /usr/local/bin/
   USER quake
   EXPOSE 8080
   HEALTHCHECK CMD curl -sf http://localhost:8080/healthz || exit 1
   ENTRYPOINT ["quake-worker"]
   ```
8. Build: `docker build -t quake-worker .`
9. Run: `docker run -v /path/to/id1:/game/id1 -e QUAKE_BASEDIR=/game quake-worker`
10. Verify: container starts, `/healthz` returns 200, SIGTERM triggers clean exit

**Rollback**: Dockerfile is additive. System layer selectable via CMake.

---

## Step 7: Azure Infrastructure Provisioning

**Input**: Container image building locally  
**Output**: Azure environment ready for deployment

1. Create `infra/main.bicep` with modules for:
   - Resource group
   - Azure Container Registry (ACR)
   - Azure Container Apps environment (with VNet)
   - Azure Files storage account (game assets)
   - Azure Key Vault
   - Azure Log Analytics workspace + Application Insights
2. Create `infra/parameters/dev.bicepparam`
3. Run: `az deployment group create --template-file infra/main.bicep --parameters infra/parameters/dev.bicepparam`
4. Push game-worker image to ACR: `az acr build -r <acr> -t quake-worker:latest .`
5. Deploy game-worker to ACA (single instance, internal only)
6. Verify: container running in ACA, logs in Log Analytics, `/healthz` succeeds

**Rollback**: `az group delete` destroys all resources.

---

## Step 8: Streaming Gateway Development

**Input**: Game worker running in ACA  
**Output**: Browser can view game stream

1. Create new service project (Go or Rust)
2. Implement worker connection: shared memory or localhost socket to game worker
3. Integrate FFmpeg: encode RGBA frames → H.264 NAL units
4. Integrate libopus: encode PCM → Opus packets
5. Implement WebRTC SFU:
   - ICE, DTLS, SRTP
   - Video track: H.264 → RTP
   - Audio track: Opus → RTP
   - Data channel: browser input → worker input injection
6. Implement signaling WebSocket endpoint
7. Build browser client: HTML page with WebRTC player
8. Containerize gateway, deploy alongside worker in same ACA container group
9. Verify: open browser → see game → hear audio → control with keyboard/mouse

**Rollback**: Gateway is a separate service; remove without affecting worker.

---

## Step 9: Session Manager Development

**Input**: Worker + gateway pair running  
**Output**: Authenticated session lifecycle management

1. Create session-manager service (C# or Go)
2. Integrate Microsoft Entra ID: OAuth 2.0 authorization code flow
3. Implement REST API: create/get/delete sessions
4. Implement worker provisioning: use ACA management API to scale worker+gateway pairs
5. Store session state in Azure Cosmos DB (or Redis)
6. Wire session → worker routing (return WebSocket URL for assigned gateway)
7. Deploy to ACA (2 replicas, public ingress via Front Door)
8. Verify: authenticate in browser → create session → play → end session → worker cleaned up

**Rollback**: Manually manage sessions; workers can run standalone.

---

## Step 10: Supporting Services

**Input**: Core platform running  
**Output**: Full platform with assets + telemetry

1. Build assets-api: serve extracted PAK contents from Azure Blob Storage + CDN
2. Build telemetry-api: ingest events, forward to Application Insights
3. Add OpenTelemetry to all services: distributed traces with W3C context propagation
4. Create Azure Monitor Workbooks dashboards
5. Set up alerting rules
6. Deploy both services to ACA

**Rollback**: Supporting services are independent; remove without affecting core gameplay.

---

## Step 11: Production Hardening

**Input**: Full platform  
**Output**: Production-ready system

1. Enable Azure Front Door with WAF v2
2. Enable Defender for Containers on ACR
3. Perform penetration test on public APIs
4. Load test: simulate 100 concurrent sessions
5. Implement canary deployment via ACA revision traffic splitting
6. Create operational runbooks
7. Final security and compliance review
8. Configure cost alerts and budgets

**Rollback**: Each hardening measure is independent and removable.
