# Technology Upgrade Plan

## Build System

### Current → Target
- **MSVC 6.0 (1998)** → **CMake 3.25+ with gcc/clang**
- **Win32 Makefile** → **CMakeLists.txt** (cross-platform)
- **Manual compilation** → **Docker multi-stage build**

### Migration Steps
1. Create root `CMakeLists.txt` with source file lists by subsystem
2. Define build options: `-DHEADLESS=ON`, `-DNOASM=ON`, `-DUSE_MESA=ON`
3. Remove MSVC 6.0 project files (`.dsw`, `.dsp`)
4. Add `Dockerfile` with builder stage (Ubuntu + build-essential + CMake) and runtime stage (distroless)
5. Validate build on Linux (primary target) and optionally macOS

## Language and Compiler

### Current → Target
- **C89 (ANSI C)** → **C11 minimum** (for `_Static_assert`, `alignof`, anonymous structs)
- **x86 inline ASM** → **Removed entirely** (use C fallbacks from `NOASM` path)
- **MSVC 6.0 extensions** → **Standard C11 with gcc/clang**

### Key Changes
| Pattern | Before | After |
| --- | --- | --- |
| Variable declarations | Top of function only (C89) | At point of use (C99+) |
| `sprintf` | `sprintf(buf, fmt, ...)` | `snprintf(buf, sizeof(buf), fmt, ...)` |
| `strcpy`/`strcat` | Unbounded copies | `strlcpy`/`strlcat` or `snprintf` |
| Assembly | `.s` files, inline `__asm` | Deleted; C equivalents |
| Platform ifdefs | `#ifdef _WIN32`, `#ifdef __linux__` | Abstraction headers (existing `vid.h`, `snd.h`, `in.h` vtables) |

## Platform Dependencies

### Video Subsystem
- **Current**: Win32 GDI (`vid_win.c`), X11 (`vid_x.c`), SVGAlib, DOS VGA
- **Target**: Mesa LLVMpipe headless OpenGL + FBO readback
- **Implementation**: New `vid_headless.c` implementing the `viddef_t` interface
  - Initialize Mesa off-screen context
  - Render to FBO
  - `VID_Update()` → copy FBO to shared framebuffer (no display blit)
  - Export function: `VID_CaptureFrame(uint8_t *rgba, int *width, int *height)`

### Sound Subsystem
- **Current**: Win32 waveOut (`snd_win.c`), OSS (`snd_linux.c`), `/dev/dsp`
- **Target**: Headless audio capture via ring buffer
- **Implementation**: New `snd_capture.c` based on `snd_null.c` pattern
  - Implement fake DMA buffer that Quake's mixer writes into
  - Expose ring buffer for external consumption
  - Export function: `SND_CaptureAudio(int16_t *pcm, int *samples, int *rate)`

### Input Subsystem
- **Current**: Win32 `WM_*` messages (`in_win.c`), X11 events, `/dev/input`
- **Target**: Programmatic input injection API
- **Implementation**: New `in_inject.c`
  - Queue-based input injection replacing OS event polling
  - Export function: `IN_InjectKeyEvent(int key, qboolean down)`
  - Export function: `IN_InjectMouseEvent(int dx, int dy, int buttons)`

### Network Subsystem
- **Current**: Berkeley sockets (`net_udp.c`, `net_wins.c`), IPX, serial/modem
- **Target**: Standard POSIX sockets (UDP only for multiplayer between workers)
- **Remove**: IPX (`net_ipx.c`), serial (`net_ser.c`), modem (`net_comx.c`)
- **Note**: For single-player cloud streaming, network is loopback-only. Multiplayer between workers uses standard UDP.

### System Layer
- **Current**: `sys_win.c`, `sys_linux.c`, `sys_dos.c`
- **Target**: Single `sys_container.c`
  - SIGTERM handler for graceful shutdown
  - Environment variable config reading
  - Structured JSON logging to stdout
  - Health check HTTP endpoint (minimal embedded HTTP or sidecar)

## Dependencies

### Removed Dependencies
| Dependency | Reason |
| --- | --- |
| Win32 API (GDI, waveOut, DirectInput) | Headless container — no Windows |
| X11 / XFree86 | Headless container — no display server |
| SVGAlib / VGA | Obsolete |
| OSS (`/dev/dsp`) | No sound hardware in container |
| IPX / Winsock 1.1 | Obsolete protocols |
| Serial/modem | Obsolete |
| DJGPP / DOS extender | Obsolete |

### New Dependencies
| Dependency | Version | Purpose |
| --- | --- | --- |
| CMake | ≥ 3.25 | Build system |
| gcc or clang | ≥ 12 | C11 compiler |
| Mesa (libGL, libEGL, libgbm) | ≥ 23.0 | Headless OpenGL via LLVMpipe |
| Docker | ≥ 24.0 | Containerization |
| FFmpeg (libavcodec, libavformat) | ≥ 6.0 | Video encoding (in streaming gateway, not in worker) |
| libopus | ≥ 1.4 | Audio encoding (in streaming gateway) |
| libwebsockets or similar | ≥ 4.3 | WebSocket transport (streaming gateway) |

### Worker Container Image Layers
```dockerfile
FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y \
    build-essential cmake \
    libgl-dev libegl-dev libgbm-dev mesa-utils
COPY . /src
WORKDIR /src
RUN cmake -B build -DHEADLESS=ON -DNOASM=ON && cmake --build build

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y \
    libgl1-mesa-dri libegl1 libgbm1 && \
    rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/quake-worker /usr/local/bin/
RUN useradd -r quake
USER quake
EXPOSE 8080
HEALTHCHECK CMD curl -f http://localhost:8080/healthz || exit 1
ENTRYPOINT ["quake-worker"]
```

## Encoding and Streaming (New: Streaming Gateway)

| Component | Technology | Notes |
| --- | --- | --- |
| Video encode | FFmpeg + libx264 or libvpx-vp9 | H.264 for broad compatibility, VP9 for quality/efficiency |
| Audio encode | libopus | 48 kHz, 64 kbps mono or stereo |
| Transport | WebRTC (preferred) or WebSocket | WebRTC for low latency; WebSocket as fallback |
| Signaling | Custom over WebSocket | Session establishment, ICE candidates |
| Browser client | HTML5 + MediaSource Extensions or WebRTC | Decode and render in browser |
