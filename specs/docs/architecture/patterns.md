# Design Patterns and Conventions

## Architectural Patterns

### 1. Platform Abstraction via Compile-Time Module Selection

The engine achieves portability by providing multiple implementations of each platform-dependent subsystem. The build system selects which source files to compile (e.g., `vid_win.c` vs `vid_x.c`) rather than using runtime polymorphism. Each platform module implements the same API defined in a header (e.g., `sys.h`, `vid.h`, `input.h`, `sound.h`).

**Example** — Video driver interface in `vid.h`:
```c
void VID_Init(unsigned char *palette);
void VID_Shutdown(void);
void VID_Update(vrect_t *rects);
int  VID_SetMode(int modenum, unsigned char *palette);
```
Implemented separately in `vid_win.c`, `vid_x.c`, `vid_svgalib.c`, `vid_dos.c`, `vid_null.c`.

### 2. Vtable-Style Function Pointer Tables (Network Drivers)

The networking layer uses C structs of function pointers as a form of runtime polymorphism — effectively manual vtables.

**`net_landriver_t`** in `net.h`:
```c
typedef struct {
    char    *name;
    qboolean initialized;
    int     controlSock;
    int     (*Init)(void);
    void    (*Shutdown)(void);
    int     (*Read)(int socket, byte *buf, int len, struct qsockaddr *addr);
    int     (*Write)(int socket, byte *buf, int len, struct qsockaddr *addr);
    int     (*Broadcast)(int socket, byte *buf, int len);
    // ... more function pointers
} net_landriver_t;
```

Similarly, `net_driver_t` provides higher-level network operations (Connect, GetMessage, SendMessage), allowing the datagram protocol (`net_dgrm.c`) and loopback (`net_loop.c`) to coexist.

### 3. Client-Server Separation in Single Process

Even single-player runs as a local server + local client communicating through the loopback network driver (`net_loop.c`). This design:
- Ensures the same code paths work for both single-player and multiplayer
- The server is always authoritative
- Enables seamless listen-server hosting (play + host)

### 4. Global State (No Object Orientation)

The engine uses **extensive global state** rather than encapsulated objects. Each subsystem owns global variables:

- `sv` (`server_t`) — current server state
- `svs` (`server_static_t`) — persistent server state
- `cl` (`client_state_t`) — current client state per-server-connection
- `cls` (`client_static_t`) — persistent client state (demo recording, connection)
- `vid` (`viddef_t`) — video buffer and dimensions
- `host_parms` (`quakeparms_t`) — startup parameters
- `realtime`, `host_frametime` — timing
- `key_dest` — current input routing (game/console/menu/message)

### 5. Command/Cvar System (Data-Driven Configuration)

The engine implements a text-based command console modeled after Unix shells:
- **Commands** (`cmd.c`): Named callbacks registered via `Cmd_AddCommand(name, function)`, executed by typing at the console
- **Cvars** (`cvar.c`): Named float/string variables registered via `Cvar_RegisterVariable()`, readable/writable from console and code
- **Aliases**: User-defined command macros
- **Config files**: Executed via `exec` command; `config.cfg` and `autoexec.cfg` processed at startup

This pattern allows runtime tuning without recompilation — players and developers can adjust rendering, physics, network, and gameplay parameters from the console.

### 6. Entity-Component Pattern (QuakeC Edicts)

Game entities (`edict_t` in `progs.h`) use a semi-ECS approach:
- **Fixed C fields** (`entvars_t` in `progdefs.h`): `origin`, `angles`, `velocity`, `classname`, `model`, `health`, `movetype`, `solid`, `think`, `touch`, `use`, etc.
- **QuakeC-defined fields**: Additional fields defined in QuakeC source, stored after the C struct
- **Variable-sized allocation**: `pr_edict_size` computed at load time
- Entity list: `sv.edicts` — flat array, indexed by entity number

### 7. BSP-Based Spatial Partitioning

The world uses Binary Space Partitioning trees (BSP) for:
- **Visibility determination**: PVS (Potential Visibility Set) precomputed and stored in BSP
- **Collision detection**: Hull-based clipping via `SV_RecursiveHullCheck()`
- **Entity linkage**: Entities linked into area nodes for spatial queries (`SV_LinkEdict()`)
- **Rendering**: Front-to-back BSP traversal with edge sorting for correct overlap

### 8. Span-Buffer Software Renderer

The software renderer uses an **edge-sorted span buffer** approach:
1. Active edges are maintained in a sorted list
2. Edges are processed scanline-by-scanline to generate horizontal spans
3. Spans are drawn with perspective-correct texture mapping (every 16 pixels)
4. A surface cache stores pre-lit, pre-mipmapped surface data to avoid redundant work

This is more efficient than z-buffering for the 1996 hardware target.

---

## Coding Conventions

### Naming Conventions

| Prefix | Meaning | Examples |
|--------|---------|---------|
| `SV_` | Server-side | `SV_Physics()`, `SV_Move()`, `SV_StartSound()` |
| `CL_` | Client-side | `CL_ParseServerMessage()`, `CL_SendCmd()` |
| `R_` | Renderer | `R_RenderView()`, `R_DrawEntitiesOnList()` |
| `GL_` / `gl_` | OpenGL renderer | `GL_Bind()`, `GL_Upload8()` |
| `D_` / `d_` | Software rasterizer | `D_DrawSurfaces()`, `D_Init()` |
| `S_` | Sound | `S_Init()`, `S_StartSound()`, `S_Update()` |
| `NET_` | Network | `NET_Init()`, `NET_SendMessage()` |
| `PR_` | QuakeC progs | `PR_ExecuteProgram()`, `PR_LoadProgs()` |
| `ED_` | Entity/edict | `ED_Alloc()`, `ED_Free()`, `ED_Print()` |
| `PF_` | QuakeC built-in functions | `PF_makevectors()`, `PF_sound()` |
| `Mod_` | Model loading | `Mod_LoadModel()`, `Mod_ForName()` |
| `Con_` | Console | `Con_Printf()`, `Con_DrawConsole()` |
| `Cmd_` | Command system | `Cmd_AddCommand()`, `Cmd_ExecuteString()` |
| `Cvar_` | Console variables | `Cvar_RegisterVariable()`, `Cvar_Set()` |
| `Key_` | Keyboard/input | `Key_Event()`, `Key_Init()` |
| `M_` | Menu | `M_Draw()`, `M_Keydown()` |
| `SCR_` | Screen | `SCR_UpdateScreen()` |
| `VID_` | Video driver | `VID_Init()`, `VID_Update()` |
| `IN_` | Input driver | `IN_Init()`, `IN_Move()` |
| `W_` | WAD file | `W_LoadWadFile()`, `W_GetLumpName()` |
| `Hunk_` | Hunk allocator | `Hunk_Alloc()`, `Hunk_AllocName()` |
| `Z_` | Zone allocator | `Z_Malloc()`, `Z_Free()` |
| `Cache_` | Cache allocator | `Cache_Alloc()`, `Cache_Check()` |
| `Q_` | Common utilities | `Q_memset()`, `Q_strcpy()`, `Q_atoi()` |
| `Host_` | Host/main loop | `Host_Frame()`, `Host_Init()` |
| `Sys_` | Platform layer | `Sys_Error()`, `Sys_FloatTime()` |
| `COM_` | Common/file system | `COM_LoadFile()`, `COM_CheckParm()` |
| `MSG_` | Message buffer | `MSG_WriteByte()`, `MSG_ReadShort()` |
| `Sbar_` | Status bar | `Sbar_Draw()` |
| `V_` | View | `V_CalcRefdef()` |
| `Cbuf_` | Command buffer | `Cbuf_AddText()`, `Cbuf_Execute()` |

### File Naming

- `subsystem.c` / `subsystem.h` — core implementation and header
- `subsystem_platform.c` — platform-specific implementation (e.g., `snd_win.c`)
- `subsystema.s` — assembly optimization of `subsystem.c` (e.g., `r_edgea.s` optimizes `r_edge.c`)
- `gl_subsystem.c` — OpenGL alternative to software renderer (e.g., `gl_rmain.c` vs `r_main.c`)

### Header Inclusion Pattern

`quakedef.h` is the **universal include** — every `.c` file includes it first. It in turn includes all other headers in dependency order:
```c
#include "common.h"    // types, endian, message API
#include "bspfile.h"   // BSP format
#include "vid.h"       // video types
#include "sys.h"       // platform API
#include "zone.h"      // memory
#include "mathlib.h"   // vectors, matrices
#include "wad.h"       // WAD files
#include "draw.h"      // 2D drawing
#include "cvar.h"      // console variables
#include "screen.h"    // screen management
#include "net.h"       // networking
#include "protocol.h"  // network protocol
#include "cmd.h"       // command system
#include "sbar.h"      // status bar
#include "sound.h"     // audio
#include "render.h"    // renderer
#include "client.h"    // client state
#include "progs.h"     // QuakeC VM
#include "server.h"    // server state
#include "model.h"     // (or gl_model.h)
#include "d_iface.h"   // rasterizer
#include "input.h"     // input
#include "world.h"     // collision
#include "keys.h"      // keyboard
#include "console.h"   // console
#include "view.h"      // view
#include "menu.h"      // menu
#include "crc.h"       // CRC
#include "cdaudio.h"   // CD audio
```

### Error Handling Pattern

- **Fatal errors**: `Sys_Error(format, ...)` — displays error, writes to log, terminates
- **Recoverable aborts**: `Host_Error(format, ...)` — uses `longjmp(host_abortserver)` to return to main loop
- **No structured exception handling** — C89 codebase relies on global state checks
- Almost no null-pointer checks or bounds validation on internal data

### Memory Management Convention

- **Never use `malloc`/`free` directly** — all allocations go through Hunk, Zone, or Cache
- Hunk allocations persist for the process lifetime or until level change
- Zone provides small dynamic allocation with explicit free
- Cache provides evictable storage (freed automatically under memory pressure)
- The `MAXGAMEDIRLEN` pattern in `console.c` (`Con_Init`) is typical: hardcoded buffer sizes with manual overflow checks

### Comment Style

- `//` single-line comments (C++ style, common in C89 pragmatically)
- `/* */` block comments for headers, file descriptions, section dividers
- `id Software comment headers` on most functions:
  ```c
  /*
  ================
  FunctionName
  ================
  */
  ```
- `FIXME:` comments mark known issues (e.g., `// FIXME: make a buffer size safe vsprintf?` in `console.c`)
- Several `TODO` and `FIXME` markers throughout indicating known technical debt

### Common Anti-Patterns (Technical Debt)

1. **Unsafe `sprintf`/`vsprintf`**: Used throughout without buffer size limits (e.g., `Con_Printf`, `Con_DebugLog`, `Host_EndGame`). `MAXPRINTMSG = 4096` is enforced only by convention, not by using `vsnprintf`.
2. **Global mutable state**: Nearly all subsystem state is in global variables, making the codebase non-reentrant and difficult to reason about.
3. **Magic numbers**: Hard-coded constants throughout (e.g., `16384` for console buffer, `8192` for signon buffer, `600` for max edicts).
4. **Incomplete error handling**: Many file operations don't check return values (e.g., `open()` result in `Con_DebugLog()` not checked for -1).
5. **Platform conditional compilation**: Deep nesting of `#ifdef _WIN32` / `#ifdef __linux__` throughout shared code.
