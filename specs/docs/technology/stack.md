# Technology Stack

## Overview

WinQuake is id Software's Quake engine source code release (1996–1997), written as a cross-platform C application with platform-specific modules and hand-optimized x86 assembly. It is a monolithic, single-binary game engine implementing a complete 3D first-person shooter with real-time software rendering, networked multiplayer, sound mixing, and a built-in scripting VM (QuakeC).

## Programming Languages

### C (ANSI C / C89)
- **Primary language** for the entire codebase
- **120+ `.c` source files**, **65+ `.h` header files**
- Compiled with MSVC 6.0 (Windows) and GCC/EGCS 1.1.2 (Linux)
- Uses C89/C90 standard features; no C99 or later constructs
- Heavy use of global state, function pointers, and preprocessor macros
- Source: All `.c` and `.h` files in `WinQuake/`

### x86 Assembly (AT&T / GAS syntax)
- **21 `.s` assembly files** providing hand-optimized inner loops
- Used for performance-critical rendering paths, sound mixing, and math
- AT&T syntax with ELF directives for Linux; separate MASM-compatible paths for Windows
- Files include: `d_draw.s`, `d_draw16.s`, `d_copy.s`, `d_parta.s`, `d_polysa.s`, `d_scana.s`, `d_spr8.s`, `d_varsa.s`, `math.s`, `r_aliasa.s`, `r_drawa.s`, `r_edgea.s`, `r_varsa.s`, `r_aclipa.s`, `surf16.s`, `surf8.s`, `snd_mixa.s`, `sys_dosa.s`, `sys_wina.s`, `worlda.s`, `dosasm.s`
- `gas2masm/` directory contains a tool for converting GAS to MASM syntax

## Target Platforms

| Platform | System Module | Video Module | Sound Module | Input Module | Network Module |
|----------|--------------|-------------|-------------|-------------|---------------|
| **Windows (Win32)** | `sys_win.c` | `vid_win.c` | `snd_win.c` | `in_win.c` | `net_wins.c`, `net_wipx.c` |
| **Windows (Dedicated)** | `sys_wind.c` | — | — | — | `net_wins.c`, `net_wipx.c` |
| **Linux (SVGALib)** | `sys_linux.c` | `vid_svgalib.c` | `snd_linux.c` | (built-in) | `net_udp.c`, `net_bsd.c` |
| **Linux (X11)** | `sys_linux.c` | `vid_x.c` | `snd_linux.c` | `in_sun.c` | `net_udp.c`, `net_bsd.c` |
| **Solaris (X11)** | `sys_sun.c` | `vid_sunx.c` | `snd_sun.c` | `in_sun.c` | `net_udp.c`, `net_bsd.c` |
| **Solaris (XIL)** | `sys_sun.c` | `vid_sunxil.c` | `snd_sun.c` | `in_sun.c` | `net_udp.c`, `net_bsd.c` |
| **DOS** | `sys_dos.c` | `vid_dos.c`, `vid_vga.c`, `vid_ext.c` | `snd_dos.c`, `snd_gus.c` | `in_dos.c` | `net_dos.c`, `net_ipx.c`, `net_ser.c`, `net_comx.c` |
| **Null (Stub)** | `sys_null.c` | `vid_null.c` | `snd_null.c` | `in_null.c` | `net_none.c` |

### OpenGL Rendering Path (GLQuake)
An alternative rendering path using OpenGL (`gl_*.c` files) is conditionally compiled via `#define GLQUAKE`:
- `gl_draw.c`, `gl_mesh.c`, `gl_model.c`, `gl_refrag.c`, `gl_rlight.c`, `gl_rmain.c`, `gl_rmisc.c`, `gl_rsurf.c`, `gl_screen.c`, `gl_test.c`, `gl_warp.c`
- Video drivers: `gl_vidnt.c` (Windows), `gl_vidlinux.c` (Linux SVGALib), `gl_vidlinuxglx.c` (Linux GLX)
- Headers: `glquake.h`, `glquake2.h`, `gl_model.h`, `gl_warp_sin.h`

## Versioning

Defined in `WinQuake/quakedef.h`:
- `VERSION`: 1.09
- `GLQUAKE_VERSION`: 1.00
- `D3DQUAKE_VERSION`: 0.01
- `WINQUAKE_VERSION`: 0.996
- `LINUX_VERSION`: 1.30
- `X11_VERSION`: 1.10
- `NET_PROTOCOL_VERSION`: 3 (in `net.h`)
- `PROTOCOL_VERSION`: 15 (in `protocol.h`)
- `BSPVERSION`: 29 (in `bspfile.h`)

## Frameworks and Libraries

### Windows Dependencies (from `WinQuake.dsp` link line)
| Library | Purpose |
|---------|---------|
| `kernel32.lib` | Windows kernel API |
| `user32.lib` | Windows UI/windowing API |
| `gdi32.lib` | Windows graphics device interface |
| `winmm.lib` | Windows multimedia (timer, wave audio) |
| `wsock32.lib` | Windows Sockets (TCP/IP networking) |
| `dxguid.lib` | DirectX GUIDs (DirectDraw, DirectSound, DirectInput) |
| `mgllt.lib` | SciTech MGL graphics library (VESA VBE video modes) |
| `opengl32.lib` / `glu32.lib` | OpenGL (GLQuake build only) |
| Standard Win32 libs | `shell32.lib`, `ole32.lib`, `advapi32.lib`, etc. |

### Linux Dependencies (from `Makefile.linuxi386`)
| Library | Purpose |
|---------|---------|
| `libm` | Math library |
| `libvga` | SVGALib console graphics |
| `libX11`, `libXext` | X11 windowing |
| `libXxf86dga` | XFree86 DGA (direct graphics access) |
| `libXxf86vm` | XFree86 VidMode extension |
| `libMesaGL` / `libGL` | Mesa/OpenGL (GLQuake only) |
| `libglide2x` | 3Dfx Glide library (3Dfx hardware only) |
| `libdl` | Dynamic loading |

### Included SDK Components
- `WinQuake/dxsdk/` — DirectX SDK headers and libraries (subset)
- `WinQuake/scitech/` — SciTech MGL Display Doctor SDK headers and libraries

## Build Systems

### Windows: Microsoft Visual C++ 6.0
- Project file: `WinQuake/WinQuake.dsp` (Visual Studio 6.0 Developer Studio Project)
- Workspace: `WinQuake/WinQuake.dsw`
- Build configurations:
  - `winquake - Win32 Release` — optimized software-rendered build
  - `winquake - Win32 Debug` — debug software-rendered build
  - `winquake - Win32 GL Release` — optimized OpenGL build
  - `winquake - Win32 GL Debug` — debug OpenGL build
- Compiler: MSVC `cl.exe` with `/G5` (Pentium optimization), `/Ox` (full optimization)
- The `.mak` file for NMAKE is not included; must be exported from Visual Studio.

### Linux: GNU Make + GCC (EGCS 1.1.2)
- Makefile: `WinQuake/Makefile.linuxi386`
- Targets:
  - `squake` — SVGALib console Quake
  - `glquake` — OpenGL via Mesa
  - `glquake.glx` — OpenGL via GLX
  - `glquake.3dfxgl` — OpenGL via 3Dfx Glide
  - `quake.x11` — X11 windowed Quake
- Compiler flags: `-mpentiumpro -O6 -ffast-math -funroll-loops -fomit-frame-pointer`

### Solaris: GNU Make
- Makefile: `WinQuake/Makefile.Solaris`
- Targets: `squake` (console), X11 version

### RPM Packaging
- `quake.spec.sh` — RPM spec generator for Linux Quake distribution
- `quake-data.spec.sh`, `quake-shareware.spec.sh`, `quake-hipnotic.spec.sh`, `quake-rogue.spec.sh` — RPM specs for game data and expansion packs

## Standard Library Usage

The engine uses standard C library functions along with custom wrappers in `common.c`:
- `Q_memset`, `Q_memcpy`, `Q_memcmp` — wrappers around `memset`/`memcpy`/`memcmp`
- `Q_strcpy`, `Q_strncpy`, `Q_strlen`, `Q_strcat` — string wrappers
- `Q_atoi`, `Q_atof` — custom number parsing with hex/octal support
- `va()` — `vsprintf` wrapper returning a static buffer for formatted strings
- Direct use of: `sprintf`, `vsprintf`, `strlen`, `strcmp`, `strcpy`, `strcat`, `strncpy`, `memset`, `memcpy`, `malloc` (indirectly via zone allocator)
