# Technical Debt Analysis

## Overview

WinQuake (1996–1997) carries extreme technical debt by modern standards. Every aspect of the codebase—language, dependencies, build tooling, architecture, and operational practices—requires fundamental modernization to achieve the target state: a cloud-native game worker running headless in Azure Container Apps with browser-based streaming.

## Debt Categories

### 1. Outdated Dependencies — CRITICAL

| Dependency | Current State | Risk | Priority |
|-----------|--------------|------|----------|
| **SciTech MGL** | Bundled binary libs; company defunct since ~2003 | Cannot compile/link on modern toolchains | **High** |
| **DirectX SDK** (DX3–5 subset) | Bundled headers from 1996 | Incompatible with modern Windows SDK | **High** |
| **SVGALib** | Requires root; removed from modern Linux distros | Blocks containerization entirely | **High** |
| **3Dfx Glide 2.x** | Hardware extinct since 2002 | Dead code | **Medium** |
| **XFree86 DGA/VidMode** | Deprecated X.org extensions | Irrelevant in headless container | **Medium** |
| **MSVC 6.0** runtime | No longer distributed | Can't build with modern MSVC without changes | **High** |
| **EGCS 1.1.2** compiler flags | `-mpentiumpro -O6` obsolete | Modern GCC uses different optimization flags | **Low** |

### 2. Deprecated Language Patterns — HIGH

| Pattern | Scope | Debt |
|---------|-------|------|
| **C89/C90 only** | 120+ .c files | No `inline`, no `bool`, no `//` comments, no VLAs, no `<stdint.h>` |
| **`sprintf`/`vsprintf`** | ~40+ call sites | Buffer overflows; must convert to `snprintf`/`vsnprintf` |
| **`strcpy`/`strcat`** | Hundreds of call sites | Unbounded copies; must convert to `strncpy`/`strlcpy` |
| **Global mutable state** | Every subsystem | `sv`, `cl`, `cls`, `svs`, `vid`, `key_dest` — thread-unsafe |
| **x86 assembly** | 21 `.s` files | Not portable; blocks ARM/container cross-compilation |
| **`setjmp`/`longjmp`** | Error recovery (`host_abortserver`) | Incompatible with modern structured error handling |

### 3. Architectural Constraints — CRITICAL

| Constraint | Impact on Cloud Target |
|-----------|----------------------|
| **Monolithic single-binary** | Cannot decompose into microservices without major refactoring |
| **Single-threaded main loop** | Cannot leverage multi-core; limits container density |
| **Frame-coupled subsystems** | Rendering, physics, sound, network all in one `Host_Frame()` |
| **No API boundaries** | Subsystems communicate through shared globals, not interfaces |
| **Client + server in one process** | Must separate for headless game worker model |
| **Platform code compiled in** | Build-time selection, not runtime abstraction |
| **Custom memory allocator** | Pool-based; complicates container memory limits/monitoring |
| **No configuration management** | Command-line args + text files; no env vars, no secrets management |

### 4. Missing Modern Infrastructure — CRITICAL

| Missing Capability | Impact |
|-------------------|--------|
| **No build system (modern)** | MSVC 6.0 .dsp and ancient Makefiles; need CMake or Meson |
| **No tests** | Zero unit, integration, or regression tests |
| **No CI/CD** | No pipeline, no automated builds |
| **No containerization** | No Dockerfile, no multi-stage build |
| **No health checks** | No liveness/readiness probes for orchestrators |
| **No structured logging** | `Con_Printf` to text buffer; no JSON, no log levels |
| **No metrics/tracing** | No OpenTelemetry, no Prometheus endpoints |
| **No configuration service** | Hard-coded values and command-line args only |
| **No graceful shutdown** | `Sys_Quit()` exits immediately |

### 5. Code Quality Issues — MEDIUM

| Issue | Scope | Debt |
|-------|-------|------|
| `FIXME` comments | Multiple (`console.c`: "make a buffer size safe vsprintf?") | Known bugs left unfixed |
| Self-modifying code | `Sys_MakeCodeWriteable()` | Disables DEP/NX protections |
| File mode 0666 | `Con_DebugLog` | World-readable log files |
| `setuid root` binaries | Linux RPM packaging | Massive privilege escalation risk |
| No input validation | Network messages, file paths, console commands | Injection/traversal vectors |
| Magic numbers | Throughout | `8000`, `1024`, `600`, `256` — undocumented limits |

## Modernization Priority Matrix

| Priority | Debt Item | Rationale |
|----------|-----------|-----------|
| **P0** | Replace build system (CMake) | Blocks all other work |
| **P0** | Remove platform display/input deps (headless mode) | Core requirement for container deployment |
| **P0** | Strip x86 assembly; C-only build | Required for container cross-compilation |
| **P1** | Replace `sprintf` → `snprintf` globally | Security prerequisite |
| **P1** | Separate server from client (headless worker) | Core architecture for cloud deployment |
| **P1** | Add Dockerfile + container build | Required for Azure Container Apps |
| **P1** | Add structured logging | Required for telemetry-api integration |
| **P2** | Add health check endpoint | Required for ACA orchestration |
| **P2** | Implement framebuffer capture API | Required for streaming gateway |
| **P2** | Replace network layer with WebSocket/gRPC input | Required for browser input |
| **P2** | Add graceful shutdown signal handling | Required for container lifecycle |
| **P3** | Thread-safety for global state | Enables multi-instance containers |
| **P3** | Replace custom allocator with standard malloc | Simplifies memory monitoring |
| **P3** | Add unit test framework | Long-term maintainability |
