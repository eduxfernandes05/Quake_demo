# Architecture Overview

## System Type

WinQuake is a **monolithic, single-process game engine** implementing a complete first-person 3D shooter. The single binary contains:

- A real-time 3D software renderer (or OpenGL hardware renderer in GLQuake builds)
- A game server with physics simulation and scripting VM
- A networked multiplayer client
- Audio mixing and playback
- A text console with command interpreter
- Platform abstraction layers for Windows, Linux, Solaris, and DOS

The client and server can run in the same process (listen server) or the server can run standalone (dedicated server via `-dedicated` flag, or `sys_wind.c` on Windows).

## High-Level Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     Platform Layer                            │
│  sys_*.c  vid_*.c  snd_*.c  in_*.c  cd_*.c  net_*.c         │
├──────────────────────────────────────────────────────────────┤
│                     Host Layer (host.c)                       │
│  Main loop · Frame timing · Init/Shutdown orchestration       │
├───────────────────────┬──────────────────────────────────────┤
│    Client Subsystem   │       Server Subsystem               │
│  cl_main.c            │  sv_main.c                           │
│  cl_parse.c           │  sv_phys.c                           │
│  cl_input.c           │  sv_move.c                           │
│  cl_demo.c            │  sv_user.c                           │
│  cl_tent.c            │  host_cmd.c                          │
├───────────────────────┴──────────────────────────────────────┤
│                   Shared Subsystems                           │
│  Rendering    │ Sound      │ Network    │ Console            │
│  r_*.c/gl_*.c │ snd_*.c    │ net_*.c    │ console.c          │
│  d_*.c        │            │            │ cmd.c / cvar.c     │
│  model.c      │            │            │ keys.c / menu.c    │
├───────────────┼────────────┼────────────┼────────────────────┤
│                   Core Services                               │
│  common.c · zone.c · mathlib.c · wad.c · crc.c · world.c    │
├──────────────────────────────────────────────────────────────┤
│                   QuakeC VM                                   │
│  pr_exec.c · pr_edict.c · pr_cmds.c · pr_comp.h · progs.h   │
└──────────────────────────────────────────────────────────────┘
```

## Execution Flow

### Startup Sequence (`sys_win.c` → `host.c`)

1. **Platform init** (`WinMain` / `main`): Parse command-line args, allocate memory block
2. **`Host_Init()`** orchestrates subsystem initialization:
   - `Cbuf_Init()` — command buffer
   - `Cmd_Init()` — command registration
   - `Cvar_Init()` — console variables
   - `COM_Init()` — file system / pak file mounting
   - `Host_InitLocal()` — host-level cvars
   - `W_LoadWadFile("gfx.wad")` — load UI graphics
   - `Key_Init()` — keyboard bindings
   - `Con_Init()` — console system
   - `M_Init()` — menu system
   - `PR_Init()` — QuakeC VM
   - `Mod_Init()` — model loader
   - `NET_Init()` — networking stack
   - `SV_Init()` — server subsystem
   - `R_Init()` — rendering subsystem (software or GL)
   - `S_Init()` — sound subsystem
   - `CDAudio_Init()` — CD audio
   - `Sbar_Init()` — status bar HUD
   - `CL_Init()` — client subsystem
   - `IN_Init()` — input devices
3. Execute startup config: `Cbuf_InsertText("exec quake.rc\n")`

### Main Loop

The main loop in `sys_win.c` (`WinMain`) or `sys_linux.c` (`main`) calls `Host_Frame()` each iteration:

```
Host_Frame(time):
  1. Sys_SendKeyEvents()        — pump OS input events
  2. Host_GetConsoleCommands()   — read stdin (dedicated)
  3. Cbuf_Execute()              — execute all queued commands
  4. NET_Poll()                  — check for network activity
  5. Host_ServerFrame()          — run server physics/game tick (if active)
     └─ SV_Physics()            — physics simulation
     └─ PR_ExecuteProgram()     — run QuakeC game logic
  6. Host_ClientFrame()          — run client update
     └─ CL_ReadFromServer()     — parse server messages
     └─ CL_SendCmd()            — send input to server
  7. SCR_UpdateScreen()          — render frame (if not dedicated)
     └─ R_RenderView()          — 3D scene rendering
     └─ Sbar_Draw()             — HUD
     └─ Con_DrawConsole()       — console overlay
     └─ M_Draw()                — menu overlay
  8. S_Update()                  — update sound positions
  9. CDAudio_Update()            — update CD playback
  10. host_framecount++
```

## Client-Server Architecture

WinQuake implements a **client-server model** even in single-player:

- **Server** (`sv_main.c`, `sv_phys.c`, `sv_move.c`, `sv_user.c`): Runs physics simulation, manages entities via QuakeC scripting VM, sends state updates to clients. Always the authority on game state.
- **Client** (`cl_main.c`, `cl_parse.c`, `cl_input.c`): Receives entity state from server, interpolates for display, captures user input and sends movement commands. Can connect to remote servers or local in-process server.
- **Connection types**:
  - `ca_dedicated` — dedicated server, no client, no rendering
  - `ca_disconnected` — client at main menu / console, not connected
  - `ca_connected` — client connected and playing

## Data Flow

### Client → Server
```
Keyboard/Mouse → IN_Move()/Key_Event() → key_lines[]/cl.cmd → CL_SendCmd()
  → NET_SendMessage() → [network] → SV_ReadClientMessage() → SV_RunClients()
```

### Server → Client  
```
QuakeC game logic → SV_SendClientMessages() → MSG_Write*() → NET_SendMessage()
  → [network] → CL_ParseServerMessage() → update cl.* state → R_RenderView()
```

### Rendering Pipeline (Software)
```
R_RenderView() → R_SetupFrame() → R_EdgeDrawing()
  → Edge sorting → Span generation → d_scan.c surface drawing
  → d_sprite.c / r_alias.c model drawing → VID_Update() → screen
```

## Memory Architecture

Defined in `zone.h` and `zone.c`, the engine uses a single large memory block (8–16 MB) subdivided into:

```
┌─────────── Top of Memory ──────────┐
│  High Hunk (video buffers, etc.)    │
│  ← high hunk pointer                │
├────────────────────────────────────┤
│  Cache (level data, textures)       │
│  ← dynamically managed              │
├────────────────────────────────────┤
│  Temp allocations (file loading)    │
├────────────────────────────────────┤
│  Low Hunk (permanent allocations)   │
│  ← low hunk pointer                 │
├────────────────────────────────────┤
│  Zone (small dynamic allocations)   │
│  ← ~48KB, zone allocator            │
└─────────── Bottom ─────────────────┘
```

- **Hunk** (`Hunk_Alloc`, `Hunk_AllocName`): Stack-like allocator, memory released by resetting pointers (e.g., between levels)
- **Zone** (`Z_Malloc`, `Z_Free`): Small-block heap allocator (~48KB) for strings, temp data
- **Cache** (`Cache_Alloc`, `Cache_Free`): LRU-evictable cache for models, sounds, textures
- **Temp** (`Hunk_TempAlloc`): Temporary file loading buffer between cache and hunk

Default memory: `MINIMUM_MEMORY` = 0x550000 (5.5MB), Windows min `MINIMUM_WIN_MEMORY` = 0x880000 (8.5MB), max `MAXIMUM_WIN_MEMORY` = 0x1000000 (16MB).

## File System

Implemented in `common.c`. The engine mounts `.pak` archive files and loose files from a directory hierarchy:

- Base directory (e.g., `c:\quake`)
- Game directory (default `id1/`, configurable via `-game` argument)
- `.pak` files are concatenated archives (`pak0.pak`, `pak1.pak`) containing game assets
- File search order: game directory loose files → pak files (highest numbered first) → base directory

## Conditional Compilation

Major compile-time configuration switches:
- `GLQUAKE` — Build OpenGL renderer instead of software renderer
- `QUAKE2` — Enable Quake II compatibility features (start spots, dark lights)
- `_WIN32` — Windows-specific code paths
- `__linux__` / `__sun__` — Unix platform paths
- `id386` — Enable x86 assembly optimizations
- `IDGODS` — Privileged network mode (development only, disabled)
- `PARANOID` — Extra debug checks (disabled for performance)
