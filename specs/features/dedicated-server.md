# Feature: Dedicated Server

## Feature ID
`dedicated-server`

## Purpose
Run the Quake server as a headless process without video, sound, or graphical input, allowing it to host multiplayer games on a remote machine. The dedicated server provides console I/O via stdin/stdout and runs the full game simulation for connected clients.

## Scope
- Headless operation (no video, sound, or mouse/keyboard input)
- Console via stdin/stdout or Win32 dedicated console
- Full server simulation (physics, QuakeC, entity management)
- Client connection management (up to 16 players)
- Remote administration via `rcon` protocol
- Configurable via console commands and config files

## User Workflows

### Launching a Dedicated Server
1. Run `quake -dedicated [maxplayers]` (default 8 players)
2. Engine skips video, sound, and input initialization
3. Console prompt appears on terminal (stdin/stdout)
4. Server loads `default.cfg`, `server.cfg` (if exists)
5. Admin types `map e1m1` to start a level
6. Server begins accepting client connections on port 26000

### Server Administration
1. Admin types commands in terminal console (e.g., `changelevel e1m2`, `kick <player>`, `status`)
2. `status` shows: hostname, map, connected players with frags and connection time
3. `kick <player>` or `kick #<id>` removes a player
4. `say <message>` broadcasts server message to all clients
5. Remote admin: player connects and uses `rcon <password> <command>` via console

### Configuration
1. Create `server.cfg` with desired settings:
   ```
   hostname "My Server"
   maxplayers 16
   deathmatch 1
   fraglimit 30
   timelimit 20
   map dm3
   ```
2. Config auto-executed on startup or via `exec server.cfg`

## Functional Requirements

### FR-DED-01: Initialization
- `-dedicated` flag sets `isDedicated` global
- `cls.state` set to `ca_dedicated`
- Skip initialization: `VID_Init`, `S_Init`, `IN_Init`, `CDAudio_Init`
- Initialize: `Host_Init`, `NET_Init`, `SV_Init`, `PR_Init`, `Cmd_Init`

### FR-DED-02: Console I/O
- **Unix**: `sys_dedicated.c` — reads from stdin, writes to stdout
- **Windows**: Separate console thread via `conproc.c`, communicates with `qhost.exe` helper process
- Non-blocking input polling each frame
- Full command parsing and execution (same as interactive console)

### FR-DED-03: Client Management
- `svs.maxclients` set from command line (default 8, max 16)
- `SV_CheckForNewClients`: Accept incoming connections each frame
- Client states: free → connected → spawned → active
- `host_client` pointer tracks currently processing client
- Disconnect handling: timeout, kick, client quit
- `host_framerate` can be set for consistent simulation rate

### FR-DED-04: Remote Console (rcon)
- `rcon_password` cvar: set on server to enable remote admin
- Client sends `rcon <password> <command>` to server
- Server validates password, executes command, returns output
- No encryption or authentication beyond password match
- Disabled when `rcon_password` is empty (default)

### FR-DED-05: Server Frame Loop
```
Dedicated Server Frame:
  1. Sys_ConsoleInput()     — read stdin commands
  2. Cbuf_Execute()         — execute buffered commands
  3. SV_Frame()             — process server:
     a. SV_CheckForNewClients()
     b. SV_ReadClientMessages()
     c. SV_Physics()        — run physics + QuakeC think
     d. SV_SendClientMessages() — send updates to clients
  4. Loop (no video/sound update)
```

### FR-DED-06: Resource Optimization
- No framebuffer allocation
- No sound mixing or DMA buffers
- No texture/model rendering data (only collision data loaded)
- Reduced memory footprint compared to listen server
- CPU usage proportional to player count and entity count

## Implementation Files

| File | Purpose |
|------|---------|
| `sys_linux.c` | Unix dedicated server: main loop, stdin console, signals |
| `sys_win.c` | Windows listen + dedicated server support |
| `sys_dos.c` | DOS system layer (rarely used for dedicated) |
| `conproc.c` | Windows dedicated console thread (IPC with qhost.exe) |
| `conproc.h` | Console process interface |
| `sv_main.c` | Server frame, client management, message dispatch |
| `sv_phys.c` | Physics simulation |
| `sv_user.c` | Player input processing |
| `host.c` | `Host_Frame` — dedicated path skips client update |
| `host_cmd.c` | Server admin commands: `status`, `kick`, `map`, etc. |

## Configuration (CVars)
| CVar | Default | Description |
|------|---------|-------------|
| `hostname` | "UNNAMED" | Server display name |
| `sv_maxspeed` | 320 | Max player speed |
| `sv_gravity` | 800 | World gravity |
| `deathmatch` | 0 | Deathmatch mode (1=enabled) |
| `coop` | 0 | Cooperative mode |
| `teamplay` | 0 | Team play rules |
| `fraglimit` | 0 | Frag limit (0=none) |
| `timelimit` | 0 | Time limit in minutes (0=none) |
| `maxplayers` | 8 | Maximum connected clients |
| `rcon_password` | "" | Remote admin password |
| `pausable` | 1 | Allow pause in multiplayer |
| `host_framerate` | 0 | Fixed frame rate (0=variable) |
| `sys_ticrate` | 0.05 | Server tick rate (seconds) |

## Command-Line Parameters
| Parameter | Description |
|-----------|-------------|
| `-dedicated [n]` | Run as dedicated server with n max players |
| `-port <n>` | Listen on UDP port n (default 26000) |
| `-game <dir>` | Load mod directory |
| `-hipnotic` | Mission pack 1 mode |
| `-rogue` | Mission pack 2 mode |

## Dependencies
- **Network** (`net_main.c`): Client connections, message transport
- **QuakeC VM** (`pr_exec.c`): All game logic execution
- **Physics** (`sv_phys.c`): Entity simulation
- **Filesystem** (`common.c`): Map, model, config loading
- **Console** (`cmd.c`, `cvar.c`): Command/cvar system

## Acceptance Criteria
- AC-01: Server starts without video/sound/input initialization
- AC-02: Console I/O functions via stdin/stdout
- AC-03: Up to 16 clients connect and play simultaneously
- AC-04: Remote console executes commands with correct password
- AC-05: Server runs stable for extended periods without memory leaks
- AC-06: `status` command displays accurate player information
- AC-07: `kick` removes specified player cleanly
- AC-08: Server gracefully handles client timeouts and disconnects
