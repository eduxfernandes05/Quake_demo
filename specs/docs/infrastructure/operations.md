# Operational Procedures

## Overview

WinQuake has no modern operational infrastructure — no monitoring, no logging framework, no health checks, no metrics, no alerting. Operations are manual and rely on console output and user observation.

## Logging

### Console Debug Log
- Enabled by command-line argument `-condebug`
- Writes all `Con_Printf` output to `<gamedir>/qconsole.log`
- Implementation: `Con_DebugLog()` in `console.c` — opens file with `O_WRONLY | O_CREAT | O_APPEND`, writes, closes per call
- The log file is deleted on startup (`unlink(temp)` in `Con_Init`)
- No log rotation, no log levels, no structured logging

### Developer Messages
- Controlled by `developer` cvar (default 0, set to 1 for verbose output)
- `Con_DPrintf()` — only prints when `developer.value` is non-zero
- Used for technical/debug messages not intended for players

### Console Output
- `Con_Printf()` — primary output to in-game console and stdout (via `Sys_Printf`)
- `Con_SafePrintf()` — safe to call when screen can't be updated (e.g., during loading)
- All console text stored in a 16KB circular buffer (`CON_TEXTSIZE`)
- Visible in-game via the drop-down console (tilde `~` key)

## Error Handling

### Fatal Errors
- `Sys_Error(format, ...)` — displays error message and terminates
  - Windows: MessageBox with error text
  - Linux: fprintf to stderr
  - Writes to `qconsole.log` if condebug active
  - Calls `Host_Shutdown()` for cleanup, then `exit(1)`

### Recoverable Errors  
- `Host_Error(format, ...)` — uses `longjmp(host_abortserver)` to return to main loop
  - Disconnects client/server
  - Doesn't terminate the process
  - Used for map loading failures, protocol errors, etc.

## Monitoring

**No monitoring exists.** The only runtime observability tools are:

### Performance Display (cvars)
| Cvar | Display |
|------|---------|
| `host_speeds 1` | Frame time breakdown: server, client, render times |
| `r_speeds 1` | Rendering statistics: polys, surfaces, edges |
| `timerefresh` | Benchmark command: rotates view 360° and reports FPS |
| `serverprofile 1` | Server frame timing |

### Network Display
| Command | Display |
|---------|---------|
| `status` | Connected players, map name, server info |
| `ping` | Player ping times |
| `net_stats` | Network statistics (if available) |

## Backup and Recovery

### Save/Load System
- `Host_Savegame_f()` / `Host_Loadgame_f()` in `host_cmd.c`
- Save files stored as text in `<gamedir>/s<n>.sav` (slots 0–11)
- Contains: map name, time, light styles, cvars, entity data
- Save file format: human-readable text with entity key-value pairs
- **No auto-save functionality**

### Configuration Persistence
- `config.cfg` written on quit via `Host_WriteConfiguration()` in `host.c`
- Contains current cvar values and key bindings
- Text format, human-editable

### Demo Recording
- `record <demoname>` — records gameplay to `.dem` file
- `playdemo <demoname>` — plays back recorded demo
- `timedemo <demoname>` — plays demo at maximum speed for benchmarking
- Network VCR (`net_vcr.c`) can record/replay raw network sessions
- Demos stored in `<gamedir>/<name>.dem`

## Process Management

### Windows
- `WinMain()` entry point in `sys_win.c`
- Single-threaded main loop (except `conproc.c` for dedicated server console)
- `Sys_Quit()` calls `Host_Shutdown()`, writes config, calls `exit(0)`
- Signal handling: none (Windows uses structured exception handling only for crashes)

### Linux
- `main()` entry point in `sys_linux.c`
- Single-threaded
- Signal handling: registers handlers for `SIGFPE`, `SIGSEGV` to `Sys_Error`
- `Sys_Quit()` calls `Host_Shutdown()`, `fcntl()` cleanup, `exit(0)`

### Dedicated Server
- Runs headless (no video, sound, or input)
- Console I/O via stdin/stdout
- Windows: `conproc.c` manages a separate console thread
- Can be run as a background process or service (manual setup)

## Known Operational Issues

1. **No graceful shutdown signal handling** — killing the process may lose unsaved configuration
2. **No watchdog/restart** — if the process crashes, it stays down
3. **Single-threaded** — a long operation (e.g., level load) blocks everything
4. **Log file recreated on every start** — no historical log preservation
5. **No resource limits** — memory usage bounded only by startup allocation, no runtime limits on entities, particles, sounds
6. **No upgrade mechanism** — must manually replace binary files
