# Performance Analysis

## Overview

WinQuake was hand-optimized for 1996 hardware (Pentium 75–200 MHz, 16 MB RAM). The target architecture—cloud container with video streaming to browsers—has fundamentally different performance characteristics. The bottlenecks shift from CPU rasterization to video encoding, network latency, and container density.

## Current Performance Profile

### CPU-Bound Operations (1996 Targets)

| Operation | Hot Path | 1996 Optimization | Cloud Relevance |
|-----------|----------|-------------------|-----------------|
| Software span rasterization | `d_draw.s`, `d_scan.c`, `surf8.s` | x86 ASM, unrolled loops | **Irrelevant** — use GPU/software Mesa |
| BSP traversal + PVS | `r_bsp.c`, `r_edge.c` | Front-to-back, edge sorting | Still CPU-bound; modern CPUs handle easily |
| Sound mixing | `snd_mix.c`, `snd_mixa.s` | x86 ASM mixing | Must capture audio buffer for streaming |
| Physics simulation | `sv_phys.c`, `world.c` | BSP hull tracing | Still needed; lightweight on modern CPUs |
| QuakeC VM | `pr_exec.c` | Switch dispatch | Acceptable performance for game logic |
| Entity updates | `sv_main.c`, `cl_parse.c` | Delta encoding | Efficient; unchanged for internal use |

### Memory Profile

| Allocation Zone | Default Size | Purpose |
|----------------|-------------|---------|
| Total pool | 16 MB | Entire engine memory |
| Zone | 48 KB | Dynamic small objects |
| Hunk (low) | ~4–8 MB | Level data, models, progs |
| Cache | ~2–4 MB | Textures, sounds (purgeable) |
| Framebuffer | ~300 KB (320×200×8bpp) | Rendering target |

At 640×480 8-bit: ~300 KB framebuffer. At 1280×720 32-bit RGBA: ~3.6 MB per frame.

## Cloud Architecture Bottlenecks

### B1: Video Encoding (NEW — Primary Bottleneck)
- **Issue**: Each game worker must encode framebuffer to H.264/VP9/AV1 for browser streaming
- **Impact**: Encoding at 720p 60fps requires significant CPU (or GPU)
- **Target**: Software encoding via FFmpeg libx264 at ~2-4 CPU cores per stream at 720p30
- **Optimization**: 
  - Use hardware encoding if GPU available (NVENC, VA-API)
  - Reduce resolution/framerate based on client bandwidth
  - Consider AV1 for better compression at same quality
  - Target 720p@30fps at 2-4 Mbps for acceptable quality

### B2: Frame Capture Latency (NEW)
- **Issue**: No API to extract framebuffer — current path blits directly to OS display
- **Impact**: Must add framebuffer capture hook between `R_RenderView()` and `VID_Update()`
- **Target**: < 1 ms overhead for framebuffer copy per frame
- **Approach**: Replace `VID_Update()` with buffer export instead of display blit

### B3: Input Latency (NEW)
- **Issue**: Browser → WebSocket → streaming gateway → game worker input pipeline adds latency
- **Impact**: Cloud gaming requires < 100 ms total input-to-photon latency
- **Target**: < 50 ms gateway-to-worker (Azure internal network)
- **Optimization**: 
  - WebSocket binary messages (not JSON)
  - Direct UDP/QUIC where possible
  - Co-locate gateway and workers in same Azure region
  - Minimize processing hops

### B4: Container Density
- **Issue**: Each player session needs a dedicated game worker container
- **Impact**: Cost scales linearly with concurrent sessions
- **Target**: Maximize sessions per node; minimize per-container overhead
- **Optimization**:
  - Minimal container image (< 100 MB)
  - Constrain memory to 256-512 MB per worker
  - 1-2 vCPU per worker (headless game + encoding)
  - Azure Container Apps auto-scaling based on active sessions

### B5: Startup Time
- **Issue**: Container cold start + level load delays session start
- **Impact**: Players wait several seconds before gameplay
- **Target**: < 5 seconds from request to first frame
- **Optimization**:
  - Pre-warm worker pool (session-manager keeps N idle workers)
  - Minimal container image for fast pull
  - Assets pre-loaded in container image or mounted volume
  - Azure Container Apps min replicas for warm pool

### B6: Audio Streaming
- **Issue**: No audio capture path exists; sound goes directly to DMA hardware
- **Impact**: Must capture mixed audio buffer for streaming alongside video
- **Target**: Opus encoding at 48 kHz stereo, < 64 kbps
- **Approach**: Intercept `S_PaintChannels()` output buffer, encode with libopus, mux into WebRTC/HLS stream

## Performance Targets for Cloud Architecture

| Metric | Target | Rationale |
|--------|--------|-----------|
| Input-to-display latency | < 100 ms end-to-end | Acceptable for FPS gameplay |
| Video encoding | 720p @ 30 fps | Balance quality vs. CPU cost |
| Audio encoding | Opus 48 kHz / 64 kbps | Low latency, good quality |
| Container memory | ≤ 512 MB | Maximize density |
| Container CPU | ≤ 2 vCPU | Headless game + encoding |
| Container startup | < 5 s (warm), < 15 s (cold) | Acceptable UX |
| Concurrent sessions per node | 8–16 per 16-vCPU node | Cost efficiency |
| Stream bitrate | 2–4 Mbps video + 64 kbps audio | Broadband users |
| API response (session create) | < 3 s | Session-manager SLA |

## Optimization Opportunities

### O1: Headless Rendering via Software Mesa (LLVMpipe)
Instead of trying to capture a framebuffer from the display pipeline, render headless:
- Use Mesa LLVMpipe (software OpenGL) in the container with no display
- EGL surfaceless context → render to FBO → read pixels → encode
- Eliminates all platform video driver dependencies
- Bonus: GLQuake path is generally faster than software renderer

### O2: Shared Asset Volume
- Mount game assets (PAK files) as a shared Azure Files volume
- All worker containers read-only mount the same volume
- No per-container asset duplication
- Container image stays small (engine binary + libs only)

### O3: Audio Buffer Direct Capture
- Replace DMA-based sound output with in-memory ring buffer
- `snd_null.c` pattern: fake DMA that `S_PaintChannels` writes into
- Encoding thread reads from ring buffer, encodes Opus, sends to gateway
- Zero hardware dependency

### O4: Network Protocol Simplification
- Internal communication (gateway↔worker) can use a simpler protocol than Quake's full net protocol
- Replace `net_loop.c` loopback with direct function calls within the worker
- Worker runs server + "virtual client" that captures state for streaming
- No serialization/deserialization overhead for internal game state
