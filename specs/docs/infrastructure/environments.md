# Environment Configuration

## Overview

WinQuake does not use environment variables, `.env` files, or any modern configuration management. Configuration is handled entirely through:
1. Command-line arguments (parsed at startup in `COM_InitArgv()`)
2. Console variables (cvars, set interactively or via config files)
3. Compile-time preprocessor defines

## Compile-Time Environment Selection

The "environment" is selected at compile time by choosing which platform modules to compile:

### Windows Client
- Includes: `sys_win.c`, `vid_win.c`, `snd_win.c`, `in_win.c`, `cd_win.c`, `net_win.c`, `net_wins.c`, `net_wipx.c`
- Defines: `_WIN32`, `WIN32`
- Links: kernel32, user32, gdi32, winmm, wsock32, dxguid, mgllt

### Windows Dedicated Server
- Includes: `sys_wind.c`, `net_win.c`, `net_wins.c`, `net_wipx.c`
- No video, sound, input, or CD audio modules

### Linux SVGALib
- Includes: `sys_linux.c`, `vid_svgalib.c`, `snd_linux.c`, `cd_linux.c`, `net_bsd.c`, `net_udp.c`
- Links: -lvga, -lm

### Linux X11
- Includes: `sys_linux.c`, `vid_x.c`, `snd_linux.c`, `cd_linux.c`, `net_bsd.c`, `net_udp.c`
- Defines: `X11`
- Links: -lX11, -lXext, -lXxf86dga, -lm

### GLQuake (any platform)
- Additional define: `GLQUAKE`
- Replaces software renderer (`r_*.c`, `d_*.c`) with OpenGL renderer (`gl_*.c`)
- Replaces `model.c` → `gl_model.c`, `model.h` → `gl_model.h`

## Runtime Configuration Variables (cvars)

Key cvars that control runtime behavior, registered in various `*_Init()` functions:

### Host / Game Settings (`host.c`)
| Cvar | Default | Description |
|------|---------|-------------|
| `host_framerate` | 0 | Fixed frame rate (0 = variable) |
| `host_speeds` | 0 | Display frame timing |
| `sys_ticrate` | 0.05 | Server tick rate (20 Hz default) |
| `developer` | 0 | Enable developer messages |
| `skill` | 1 | Game difficulty (0–3) |
| `deathmatch` | 0 | Deathmatch mode (0, 1, 2) |
| `coop` | 0 | Cooperative mode |
| `teamplay` | 0 | Team play mode |
| `fraglimit` | 0 | Frag limit for deathmatch |
| `timelimit` | 0 | Time limit for deathmatch |
| `pausable` | 1 | Allow pausing |
| `samelevel` | 0 | Stay on same level after deathmatch round |
| `noexit` | 0 | Prevent level exit in deathmatch |

### Server Physics (`sv_main.c`)
| Cvar | Default | Description |
|------|---------|-------------|
| `sv_gravity` | 800 | Gravity strength |
| `sv_maxvelocity` | 2000 | Maximum entity velocity |
| `sv_friction` | 4 | Ground friction |
| `sv_edgefriction` | 2 | Edge friction (prevents falling off ledges easily) |
| `sv_stopspeed` | 100 | Speed below which entities stop |
| `sv_maxspeed` | 320 | Maximum player speed |
| `sv_accelerate` | 10 | Player acceleration |
| `sv_nostep` | 0 | Disable automatic step-up |
| `sv_aim` | 0.93 | Vertical auto-aim |

### Console (`console.c`)
| Cvar | Default | Description |
|------|---------|-------------|
| `con_notifytime` | 3 | Seconds to display notify messages |

### Rendering (various `r_*.c`)
| Cvar | Default | Description |
|------|---------|-------------|
| `r_draworder` | 0 | Draw order control |
| `r_speeds` | 0 | Display rendering statistics |
| `r_drawflat` | 0 | Draw without textures |
| `r_ambient` | 0 | Ambient light level |
| `r_clearcolor` | 2 | Background clear color |
| `screensize` | varies | View size (sbar_lines) |

### Sound
| Cvar | Default | Description |
|------|---------|-------------|
| `volume` | 0.7 | Sound volume |
| `bgmvolume` | 1 | Music volume |

### Network
| Cvar | Default | Description |
|------|---------|-------------|
| `hostname` | varies | Server host name |

## Game Data Directory Structure

```
<basedir>/                    # e.g., c:\quake
├── id1/                      # Base game (GAMENAME = "id1")
│   ├── pak0.pak              # Core assets (maps, models, sounds, textures)
│   ├── pak1.pak              # Additional registered game assets
│   ├── config.cfg            # Saved configuration
│   ├── default.cfg           # Default bindings
│   ├── quake.rc              # Startup script
│   └── maps/                 # Loose map files (override pak)
├── hipnotic/                 # Scourge of Armagon expansion
│   └── pak0.pak
├── rogue/                    # Dissolution of Eternity expansion
│   └── pak0.pak
└── <moddir>/                 # Custom mod directory (-game <moddir>)
    ├── pak0.pak              # Mod assets
    ├── progs.dat             # Compiled QuakeC game logic
    └── maps/                 # Mod maps
```

## Memory Configuration

Memory is allocated as a single contiguous block at startup:

| Parameter | Windows | Linux | DOS |
|-----------|---------|-------|-----|
| Minimum | 8.5 MB | 5.5 MB | 5.5 MB |
| Default | ~12 MB | ~8 MB | ~8 MB |
| Maximum | 16 MB | (system limit) | 16 MB |
| Override | `-heapsize <kb>` | `-heapsize <kb>` | `-heapsize <kb>` |
| Zone override | `-zone <bytes>` | `-zone <bytes>` | `-zone <bytes>` |
