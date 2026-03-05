# Dependencies Analysis

## Overview

WinQuake has **zero runtime package-manager dependencies** (no npm, pip, NuGet, etc.). All dependencies are either OS-provided system libraries, bundled SDK subsets, or standard C library functions. This is characteristic of 1996-era C game development.

## External Library Dependencies

### Windows Build Dependencies

| Library | Version | Purpose | Source | Status (2026) |
|---------|---------|---------|--------|---------------|
| **DirectX SDK** (subset) | ~DirectX 3–5 era | DirectDraw (video), DirectSound (audio), DirectInput (mouse) | Bundled in `WinQuake/dxsdk/` | **Obsolete** — superseded by DirectX 12, DXGI |
| **SciTech MGL** | Unknown (late 1990s) | VESA VBE 2.0 / VBE/AF video mode support | Bundled in `WinQuake/scitech/` | **Defunct** — SciTech Software no longer exists |
| **Windows SDK** | Win95/NT 4.0 era | Win32 API (kernel32, user32, gdi32, winmm, wsock32, etc.) | System | Still available but API surface evolved significantly |
| **MSVC Runtime** | MSVC 6.0 (1998) | C standard library | System | **Obsolete** — MSVC 6.0 runtime no longer distributed |

### Linux Build Dependencies

| Library | Version | Purpose | Source | Status (2026) |
|---------|---------|---------|--------|---------------|
| **glibc** | libc6 (1990s) | C standard library | System | Current, still compatible |
| **libvga** (SVGALib) | 1.x | Console-mode graphics | System | **Defunct** — SVGALib abandoned, not in modern distros |
| **libX11** | X11R6 | X Window System client library | System | Still available, but Wayland replacing X11 |
| **libXext** | X11R6 | X11 extensions | System | Available |
| **libXxf86dga** | XFree86 3.x | Direct Graphics Access | System | **Deprecated** — removed from modern X.org |
| **libXxf86vm** | XFree86 3.x | Video Mode extension | System | Still available in X.org |
| **Mesa/libGL** | Mesa 2.6 | OpenGL implementation | System | Current (modern Mesa 23.x+) |
| **libglide2x** | Glide 2.x | 3Dfx Voodoo hardware API | System | **Extinct** — 3Dfx hardware no longer exists |
| **libdl** | glibc | Dynamic library loading | System | Current |
| **libm** | glibc | Math library | System | Current |

### Bundled Dependencies

Located within the source tree:

| Directory | Contents | Purpose |
|-----------|----------|---------|
| `WinQuake/dxsdk/` | DirectX headers and import libraries | DirectDraw, DirectSound, DirectInput GUIDs and interfaces |
| `WinQuake/scitech/` | SciTech MGL headers and libraries | VESA VBE video mode access on Windows |
| `WinQuake/gas2masm/` | Assembly translator tool | Convert GAS (AT&T) assembly to MASM (Intel) syntax |
| `WinQuake/kit/` | Distribution kit files | Installation/packaging support files |
| `WinQuake/data/` | Data files | Game data/assets directory |
| `WinQuake/docs/` | Documentation | Internal documentation |

## Standard Library Usage

The engine relies on standard C library functions, with custom wrappers defined in `common.c`:

### Custom Wrappers
| Wrapper | Standard Function | Notes |
|---------|-------------------|-------|
| `Q_memset()` | `memset()` | Direct wrapper |
| `Q_memcpy()` | `memcpy()` | Direct wrapper |
| `Q_memcmp()` | `memcmp()` | Direct wrapper |
| `Q_strcpy()` | `strcpy()` | Direct wrapper |
| `Q_strncpy()` | `strncpy()` | Direct wrapper |
| `Q_strlen()` | `strlen()` | Direct wrapper |
| `Q_strrchr()` | `strrchr()` | Direct wrapper |
| `Q_strcat()` | `strcat()` | Direct wrapper |
| `Q_strcmp()` | `strcmp()` | Direct wrapper |
| `Q_strncmp()` | `strncmp()` | Direct wrapper |
| `Q_atoi()` | Custom | Parses decimal, hex (0x), octal (0) |
| `Q_atof()` | Custom | Parses decimal floats, hex, octal |
| `va()` | `vsprintf` to static buffer | Returns formatted string |

### Direct Standard Library Usage
- `<math.h>`: `sin()`, `cos()`, `atan2()`, `sqrt()`, `fabs()`, `floor()`, `ceil()`
- `<string.h>`: `strcmp()`, `strcpy()`, `strlen()`, `strcat()`, `strncpy()`, `memset()`, `memcpy()`
- `<stdio.h>`: `sprintf()`, `vsprintf()`, `fopen()`, `fclose()`, `fread()`, `fwrite()`, `fprintf()`, `printf()`
- `<stdlib.h>`: `atoi()`, `atof()`, `malloc()` (only indirectly through zone)
- `<stdarg.h>`: `va_list`, `va_start()`, `va_end()`
- `<setjmp.h>`: `setjmp()`, `longjmp()` — used for `host_abortserver` error recovery
- `<fcntl.h>`: `open()`, `O_WRONLY`, `O_CREAT`, `O_APPEND`
- `<unistd.h>`: `read()`, `write()`, `close()`, `unlink()` (Unix)
- `<errno.h>`: Error codes

## Dependency Security Assessment

### Critical Risks
1. **SciTech MGL**: Company defunct, libraries unmaintained since ~2003. Binary-only. Cannot be updated.
2. **DirectX SDK subset**: Ancient version with known API issues. Modern DirectX is fundamentally different.
3. **3Dfx Glide**: Hardware manufacturer bankrupted in 2002. Libraries non-functional on modern hardware.
4. **SVGALib**: Requires root privileges, abandoned upstream, removed from most Linux distributions.
5. **`vsprintf`/`sprintf`**: Used pervasively without bounds checking (see security.md).

### Dependency Age
All dependencies are from the 1996–1999 era. None have received security updates in over 20 years. The bundled SDK subsets are frozen in time within the repository.

## Code Organization Quality

### Positive Patterns
- Clean separation between platform-specific and platform-independent code
- Header files properly declare public interfaces
- Consistent naming conventions throughout
- Single universal include (`quakedef.h`) prevents header ordering issues

### Issues
- No dependency management system (all manual, bundled, or OS-provided)
- No version pinning or compatibility checking
- Tight coupling between subsystems through global state
- Assembly files create strong x86 architecture dependency
- No automated testing infrastructure whatsoever

## Modernization Implications

To modernize this codebase, the following dependencies would need replacement:

| Original | Modern Replacement |
|----------|-------------------|
| SciTech MGL | SDL2 or direct platform APIs |
| DirectDraw/DirectSound/DirectInput | SDL2, or modern DirectX/Vulkan |
| SVGALib | SDL2, Wayland/X11 |
| 3Dfx Glide | Vulkan, modern OpenGL |
| XFree86 DGA/VidMode | SDL2, modern X.org/Wayland |
| Raw BSD sockets | SDL_net or modern networking library |
| Custom memory allocator | Consider standard allocators or jemalloc |
| `sprintf`/`vsprintf` | `snprintf`/`vsnprintf` |
| x86 assembly | Compiler intrinsics or let modern compilers optimize |
