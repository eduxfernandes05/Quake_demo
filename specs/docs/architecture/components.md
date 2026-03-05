# Component Architecture

## Component Inventory

The WinQuake codebase consists of approximately 120 C source files, 65 header files, and 21 assembly files organized into the following functional subsystems.

---

## 1. Host / Main Loop

The host layer orchestrates all subsystem initialization, the main game loop frame timing, and shutdown.

| File | Role |
|------|------|
| `host.c` | `Host_Init()`, `Host_Frame()`, `Host_Shutdown()` — main loop, subsystem coordination |
| `host_cmd.c` | Host-level console commands: `status`, `quit`, `god`, `noclip`, `fly`, `map`, `changelevel`, `connect`, `reconnect`, `name`, `color`, `kill`, `pause`, `save`, `load`, `begin`, `kick`, `ping` |

### Key State
- `host_parms` — base directory, command-line arguments, memory block
- `host_frametime` — per-frame delta time
- `realtime` — accumulated real time
- `host_framecount` — frame counter

---

## 2. Platform Abstraction Layer (sys_*.c)

Provides OS-specific implementations of file I/O, timing, error handling, and the process entry point.

| File | Platform |
|------|----------|
| `sys_win.c` | Windows (client) — `WinMain`, `Sys_FloatTime` (QueryPerformanceCounter), file I/O, memory allocation via `GlobalAlloc` |
| `sys_wind.c` | Windows (dedicated server) — console-mode entry point |
| `sys_linux.c` | Linux — `main`, `Sys_FloatTime` (gettimeofday), signal handling |
| `sys_sun.c` | Solaris — similar to Linux with Solaris-specific timing |
| `sys_dos.c` | DOS — real-mode/protected-mode interface, DPMI |
| `sys_null.c` | Null/stub implementation for porting reference |
| `conproc.c` / `conproc.h` | Windows dedicated server console management (separate thread) |

### System Interface (`sys.h`)
- `Sys_FileOpenRead/Write/Close/Seek/Read/Write` — file I/O
- `Sys_FloatTime()` — high-resolution timer
- `Sys_Error()` — fatal error with message box
- `Sys_Printf()` — debug console output
- `Sys_Quit()` — clean shutdown
- `Sys_SendKeyEvents()` — pump OS input events
- `Sys_MakeCodeWriteable()` — memory protection for self-modifying code

---

## 3. Video / Display Subsystem

### Software Renderer

| File | Role |
|------|------|
| `r_main.c` | Software renderer entry point — `R_RenderView()`, view setup, entity rendering dispatch |
| `r_bsp.c` | BSP tree traversal and surface rendering |
| `r_edge.c` | Edge-sorted rendering — the core span generation engine |
| `r_surf.c` | Surface cache and texture mapping |
| `r_alias.c` | Alias model (character/weapon) rendering |
| `r_sprite.c` | Sprite rendering |
| `r_light.c` | Dynamic lighting calculations |
| `r_sky.c` | Sky rendering |
| `r_draw.c` | Low-level drawing routines |
| `r_misc.c` | Renderer initialization, particle rendering |
| `r_part.c` | Particle system |
| `r_efrag.c` | Entity fragment management (entity-to-leaf linking) |
| `r_aclip.c` | Alias model clipping |

### Software Rasterizer (`d_*.c`)

| File | Role |
|------|------|
| `d_edge.c` | Edge table management and scanning |
| `d_scan.c` | Span drawing — texture-mapped surface scanlines |
| `d_sprite.c` | Sprite scanline drawing |
| `d_polyse.c` | Polygon scanline edge drawing |
| `d_sky.c` | Sky dome rendering |
| `d_surf.c` | Surface cache management |
| `d_fill.c` | Flat-color fill |
| `d_part.c` | Particle drawing |
| `d_zpoint.c` | Z-buffered point drawing |
| `d_init.c` | Rasterizer initialization |
| `d_modech.c` | Video mode change handling |
| `d_vars.c` | Shared rasterizer variables |

### 2D Drawing

| File | Role |
|------|------|
| `draw.c` | 2D graphics — `Draw_Character()`, `Draw_String()`, `Draw_Pic()`, `Draw_ConsoleBackground()` |
| `screen.c` | Screen management — `SCR_UpdateScreen()`, loading plaque, screenshot |
| `sbar.c` | Status bar / HUD rendering — health, ammo, armor, weapons, scoreboard |

### Video Drivers (`vid_*.c`)

| File | Platform | Technology |
|------|----------|-----------|
| `vid_win.c` | Windows | DirectDraw + DIB + MGL/VESA |
| `vid_svgalib.c` | Linux | SVGALib console |
| `vid_x.c` | Linux | X11 + MIT-SHM |
| `vid_sunx.c` | Solaris | X11 |
| `vid_sunxil.c` | Solaris | XIL (X Imaging Library) |
| `vid_dos.c` | DOS | VGA/VESA/MGL |
| `vid_vga.c` | DOS | Direct VGA register programming |
| `vid_ext.c` | DOS | Extended video modes |
| `vid_null.c` | Stub | No-op |

### OpenGL Renderer (`gl_*.c`)

| File | Role |
|------|------|
| `gl_rmain.c` | GL renderer main — `R_RenderView()` (GL path) |
| `gl_rsurf.c` | GL BSP surface rendering |
| `gl_rlight.c` | GL dynamic lighting |
| `gl_rmain.c` | GL view setup and entity rendering |
| `gl_rmisc.c` | GL initialization and misc |
| `gl_mesh.c` | GL alias model mesh rendering |
| `gl_refrag.c` | GL entity fragment management |
| `gl_draw.c` | GL 2D drawing |
| `gl_screen.c` | GL screen management |
| `gl_warp.c` | GL water/sky warp effects |
| `gl_test.c` | GL test/debug functions |
| `gl_model.c` | GL model loading (extended model struct) |
| `gl_vidnt.c` | GL video driver (Windows) |
| `gl_vidlinux.c` | GL video driver (Linux) |
| `gl_vidlinuxglx.c` | GL video driver (Linux GLX) |

### Key Headers
- `render.h` — entity definition, refresh interface
- `r_local.h` / `r_shared.h` — internal renderer structures
- `d_iface.h` / `d_local.h` — rasterizer interface/internals
- `vid.h` — `viddef_t` structure (buffer, dimensions, palette)
- `draw.h` — 2D drawing API

---

## 4. Sound Subsystem

| File | Role |
|------|------|
| `snd_dma.c` | Sound engine core — `S_Init()`, `S_StartSound()`, `S_Update()`, channel management, spatialization |
| `snd_mix.c` | Audio mixing — `S_PaintChannels()`, sample interpolation, 8-bit/16-bit mixing |
| `snd_mem.c` | Sound loading — WAV parsing, sample rate conversion, cache management |

### Sound Drivers

| File | Platform |
|------|----------|
| `snd_win.c` | Windows — DirectSound + wave output fallback |
| `snd_linux.c` | Linux — `/dev/dsp` (OSS) |
| `snd_sun.c` | Solaris — `/dev/audio` |
| `snd_dos.c` | DOS — Sound Blaster, DMA programming |
| `snd_gus.c` | DOS — Gravis Ultrasound |
| `snd_next.c` | NeXTSTEP |
| `snd_null.c` | Stub — no sound |

### Key Header: `sound.h`
- `channel_t` — sound channel (entity, position, volume, sfx)
- `sfx_t` — sound effect reference (name + cache)
- `sfxcache_t` — cached sound data (samples, rate, looping)
- `dma_t` — DMA buffer description

---

## 5. Input Subsystem

| File | Platform |
|------|----------|
| `in_win.c` | Windows — DirectInput mouse + Win32 keyboard |
| `in_dos.c` | DOS — direct hardware mouse/keyboard |
| `in_sun.c` | Solaris — X11 input |
| `in_null.c` | Stub |
| `keys.c` | Key event processing, key binding, command line editing |

### Key Header: `input.h`, `keys.h`
- `IN_Init()`, `IN_Shutdown()`, `IN_Move()` — input device interface
- `Key_Event()` — process key up/down events
- Key destinations: `key_game`, `key_console`, `key_message`, `key_menu`

---

## 6. Network Subsystem

### Architecture: Layered driver model with protocol abstraction

| File | Role |
|------|------|
| `net_main.c` | Network core — initialization, connection management, message dispatch |
| `net_dgrm.c` | Datagram protocol — reliable/unreliable message layer on top of transport |
| `net_loop.c` | Loopback driver for local (single-player) connections |
| `net_vcr.c` | Network VCR — recording/playback of network sessions |

### Transport Drivers

| File | Protocol | Platform |
|------|----------|----------|
| `net_udp.c` | UDP/IP | Unix (BSD sockets) |
| `net_bsd.c` | BSD socket wrapper | Unix |
| `net_wins.c` | Winsock TCP/IP | Windows |
| `net_wipx.c` | Winsock IPX/SPX | Windows |
| `net_ipx.c` | Direct IPX | DOS |
| `net_ser.c` | Serial (modem/null-modem) | DOS |
| `net_comx.c` | COM port driver | DOS |
| `net_mp.c` | MPATH network provider | DOS |
| `net_bw.c` | BattleWire network provider | DOS |
| `net_dos.c` | DOS network wrapper | DOS |
| `net_win.c` | Windows network wrapper | Windows |
| `net_wso.c` | Winsock operations | Windows |
| `net_none.c` | Stub | Any |

### Key Header: `net.h`
- `qsocket_t` — socket abstraction with reliable/unreliable message queues
- `net_landriver_t` — low-level transport driver vtable (Init, Read, Write, Broadcast, etc.)
- `net_driver_t` — high-level network driver vtable (Connect, GetMessage, SendMessage, etc.)

---

## 7. Console / Command System

| File | Role |
|------|------|
| `console.c` | Console display — text buffer, `Con_Printf()`, `Con_Print()`, drawing, notify lines |
| `cmd.c` | Command buffer and execution — `Cbuf_AddText()`, `Cbuf_Execute()`, `Cmd_AddCommand()`, alias system |
| `cvar.c` | Console variable system — `Cvar_RegisterVariable()`, `Cvar_Set()`, config persistence |
| `keys.c` | Key binding, command-line editing, key event routing to game/console/menu/message |

### Key Concepts
- Commands are text strings executed via `Cmd_ExecuteString()`
- Sources: `src_client` (local), `src_command` (console), or forwarded to server
- `cvar_t` holds name, string value, float cache, archive flag, server-notify flag
- Console text is a fixed 16KB circular buffer (`CON_TEXTSIZE = 16384`)

---

## 8. Menu System

| File | Role |
|------|------|
| `menu.c` | Complete menu system — Main, Single Player, Multiplayer, Options, Video, Keys, Help, Quit, Network config, Game options, Server browser |

### Menu States (from `menu.c`)
`m_none`, `m_main`, `m_singleplayer`, `m_load`, `m_save`, `m_multiplayer`, `m_setup`, `m_net`, `m_options`, `m_video`, `m_keys`, `m_help`, `m_quit`, `m_serialconfig`, `m_modemconfig`, `m_lanconfig`, `m_gameoptions`, `m_search`, `m_slist`

Each menu state has: `M_Menu_*_f()` (entry), `M_*_Draw()` (render), `M_*_Key()` (input handler).

---

## 9. QuakeC Virtual Machine

| File | Role |
|------|------|
| `pr_exec.c` | QuakeC bytecode interpreter — `PR_ExecuteProgram()`, stack management, debug tracing |
| `pr_edict.c` | Entity management — `ED_Alloc()`, `ED_Free()`, entity loading from BSP, save/load |
| `pr_cmds.c` | Built-in functions exposed to QuakeC — e.g., `PF_makevectors`, `PF_setorigin`, `PF_sound`, `PF_traceline`, `PF_sprint`, `PF_centerprint`, `PF_ambientsound`, `PF_precache_*` |

### Key Headers
- `pr_comp.h` — QuakeC opcodes, instruction format, type system
- `progs.h` — edict structure, program header, global variables
- `progdefs.h` — auto-generated struct mapping QuakeC globals/fields to C (`globalvars_t`, `entvars_t`)

---

## 10. Model / Asset Loading

| File | Role |
|------|------|
| `model.c` | BSP world model loading, alias model (MDL) loading, sprite loading, submodel extraction |
| `gl_model.c` | Same as `model.c` but with GL-specific extensions (texture upload) |
| `wad.c` | WAD2 file loading (UI graphics, console characters) |
| `common.c` | PAK archive file system, file search paths, endian handling |

### Key Headers
- `model.h` / `gl_model.h` — BSP structures (`mnode_t`, `mleaf_t`, `msurface_t`, `mplane_t`), alias model structures, sprite structures
- `modelgen.h` — Alias model on-disk format (MDL)
- `spritegn.h` — Sprite on-disk format (SPR)
- `bspfile.h` — BSP file format (version 29), lump definitions
- `wad.h` — WAD2 file format

---

## 11. World / Physics

| File | Role |
|------|------|
| `world.c` | BSP hull collision, spatial partitioning (AABB tree), `SV_Move()` trace, point contents |
| `sv_phys.c` | Server physics — gravity, movement types (walk, fly, noclip, toss, bounce, push), trigger touching |
| `sv_move.c` | Monster movement — `SV_movestep()`, pathfinding, step up/down, `SV_NewChaseDir()` |
| `chase.c` | Third-person chase camera |

### Key Header: `world.h`
- `trace_t` — collision trace result (fraction, endpos, normal, hit entity)
- Movement types: `MOVE_NORMAL`, `MOVE_NOMONSTERS`, `MOVE_MISSILE`

---

## 12. Math Utilities

| File | Role |
|------|------|
| `mathlib.c` | Vector math, angle conversion, `R_ConcatRotations()`, `R_ConcatTransforms()`, `BoxOnPlaneSide()` |
| `math.s` | Assembly-optimized `Invert24To16` |

### Key Header: `mathlib.h`
- `vec3_t` — 3-component float vector
- `VectorAdd`, `VectorSubtract`, `VectorCopy`, `DotProduct`, `CrossProduct` macros
- `AngleVectors()` — convert Euler angles to forward/right/up vectors

---

## 13. CD Audio

| File | Platform |
|------|----------|
| `cd_audio.c` | Shared CD audio logic |
| `cd_win.c` | Windows MCI CD audio |
| `cd_linux.c` | Linux CDROM ioctl |
| `cd_null.c` | Stub |

---

## 14. View / Camera

| File | Role |
|------|------|
| `view.c` | View calculations — bob, roll, damage kicks, color shifts (water/lava/slime), idle drift, blend effects |

---

## 15. Utility / Support

| File | Role |
|------|------|
| `common.c` | File system, PAK loading, endian functions, string utilities, message read/write, `COM_Init()` |
| `zone.c` | Memory management — Hunk, Zone, Cache, Temp allocators |
| `crc.c` | CRC-16 computation (used for model/map validation) |
| `nonintel.c` | Non-x86 fallback implementations for assembly routines |
| `vregset.c` | VGA register set programming (DOS) |
