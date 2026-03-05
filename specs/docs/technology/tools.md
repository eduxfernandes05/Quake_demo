# Development and Build Tools

## Build Systems

### Windows: Microsoft Visual C++ 6.0

- **Project files**: `WinQuake.dsp` (Developer Studio Project), `WinQuake.dsw` (workspace)
- **Additional project files**: `WinQuake.mdp` (old-format project), `WinQuake.ncb` (IntelliSense database), `WinQuake.opt` (user options), `WinQuake.plg` (build log)
- **Resource file**: `winquake.rc` — Windows resource definitions (icons, version info)
- **Icons**: `quake.ico`, `qe3.ico`
- **Build tool**: `cl.exe` (MSVC compiler), `link.exe` (linker), `bscmake.exe` (browse info)
- **No NMAKE export**: The `.mak` file is not checked in; it must be generated via "Export Makefile" in Visual Studio IDE

#### Build Configurations

| Configuration | Preprocessor | Optimization | Output |
|--------------|-------------|-------------|--------|
| Win32 Release | `WIN32`, `NDEBUG`, `_WINDOWS` | `/G5 /Ox /Ot /Ow` (Pentium, full opt) | `.\Release\` |
| Win32 Debug | `WIN32`, `_DEBUG`, `_WINDOWS` | `/Od /ZI` (none, debug info) | `.\Debug\` |
| Win32 GL Release | `WIN32`, `NDEBUG`, `_WINDOWS`, `GLQUAKE` | as Release | `.\Release_gl\` |
| Win32 GL Debug | `WIN32`, `_DEBUG`, `_WINDOWS`, `GLQUAKE` | as Debug | `.\debug_gl\` |

#### Include Paths
- `.\scitech\include` — SciTech MGL headers
- `.\dxsdk\sdk\inc` — DirectX SDK headers

### Linux: GNU Make + EGCS

- **Makefile**: `Makefile.linuxi386`
- **Compiler**: EGCS 1.1.2 (early GCC fork, path: `/usr/local/egcs-1.1.2/bin/gcc`)
- **Release flags**: `-mpentiumpro -O6 -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations`
- **Debug flags**: `-g`
- **Link flags**: `-lm`
- **Base define**: `-Dstricmp=strcasecmp` (maps MSVC `stricmp` to POSIX `strcasecmp`)

#### Linux Build Targets

| Target | Binary | Video | Description |
|--------|--------|-------|-------------|
| `squake` | `$(BUILDDIR)/bin/squake` | SVGALib | Console-mode Quake |
| `glquake` | `$(BUILDDIR)/bin/glquake` | Mesa OpenGL | OpenGL Quake (Mesa) |
| `glquake.glx` | `$(BUILDDIR)/bin/glquake.glx` | GLX | OpenGL Quake (GLX) |
| `glquake.3dfxgl` | `$(BUILDDIR)/bin/glquake.3dfxgl` | 3Dfx Glide | OpenGL Quake (3Dfx) |
| `quake.x11` | `$(BUILDDIR)/bin/quake.x11` | X11 | X11 windowed Quake |
| `unixded` | (commented out) | None | Unix dedicated server |

#### Build Directory Convention
- Debug: `debugi386glibc/` or `debugi386/`
- Release: `releasei386glibc/` or `releasei386/`
- Auto-detected based on `uname -m` (i386 vs alpha) and glibc presence

### Solaris: GNU Make

- **Makefile**: `Makefile.Solaris`
- Targets similar to Linux but with Solaris-specific libraries

### RPM Packaging

- `quake.spec.sh` — Shell script that generates RPM spec files
- `quake-data.spec.sh` — Game data RPM
- `quake-shareware.spec.sh` — Shareware version RPM
- `quake-hipnotic.spec.sh` — Hipnotic expansion RPM
- `quake-rogue.spec.sh` — Rogue expansion RPM

### Utility Tools

| File | Purpose |
|------|---------|
| `gas2masm/` | GAS-to-MASM assembly syntax converter |
| `clean.bat` | Windows cleanup batch file |
| `makezip.bat` | Windows distribution ZIP creator |
| `q.bat`, `qa.bat`, `qb.bat`, `qt.bat`, `wq.bat` | Windows batch launchers with preset configurations |

## Testing Infrastructure

**There is no automated testing infrastructure.**

- No unit tests
- No integration tests
- No test harness or framework
- No CI/CD pipeline
- No code coverage tooling
- No static analysis configuration
- No linting rules

Testing was entirely manual through gameplay and developer playtesting, which was standard for 1996 game development.

## Documentation

### In-Repository Documentation
| File | Contents |
|------|----------|
| `wqreadme.txt` | WinQuake user documentation — installation, configuration, troubleshooting, video/sound/network notes |
| `README.Solaris` | Solaris build/run instructions |
| `glqnotes.txt` | GLQuake release notes |
| `3dfx.txt` | 3Dfx-specific information |

### Code Documentation
- Most functions have a `/* FunctionName */` block comment above them
- Some headers have detailed usage documentation (e.g., `cvar.h` explains the cvar system, `zone.h` explains memory architecture, `cmd.h` explains command buffer operation)
- `FIXME:` comments mark known issues (found in `console.c`, `quakedef.h`, `common.h`, etc.)
- No generated API documentation (no Doxygen, no man pages)

## Version Control

- No version control metadata in the repository (no `.git` history from original development)
- This is a source release snapshot from id Software's internal development
- The original development used a custom version control system

## Development Workflow (As Evidenced by Code)

1. **Edit source** in Visual Studio 6.0 (Windows) or vi/emacs (Unix)
2. **Build** via IDE (Windows) or `make` (Unix)
3. **Test** by running the game manually
4. **Package** via `makezip.bat` (Windows) or RPM spec scripts (Linux)
5. **Debug** via Visual Studio debugger or `printf`-debugging (`Con_DPrintf`, `developer` cvar)
