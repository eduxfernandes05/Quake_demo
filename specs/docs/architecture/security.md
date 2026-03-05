# Security Architecture and Patterns

## Overview

WinQuake was developed in 1996–1997 for a trusted desktop environment. It predates modern security practices and contains numerous patterns that would be considered vulnerabilities by today's standards. There is **no security architecture** in the modern sense — no authentication, no encryption, no input sanitization framework, and no sandboxing.

## Network Security

### No Authentication
- There is **no player authentication mechanism**. Players connect with a name and color (`host_cmd.c`: `Host_Name_f`, `Host_Color_f`) but no credentials.
- The `IDGODS` define in `quakedef.h` would grant privileged status to connections from id Software's network — disabled in the release:
  ```c
  // This makes anyone on id's net privileged
  // Use for multiplayer testing only - VERY dangerous!!!
  // #define IDGODS
  ```
- Server commands (`rcon`) are not implemented in this version.

### No Encryption
- All network traffic is sent in **plaintext** over UDP (protocol version 15).
- The datagram protocol (`net_dgrm.c`) provides reliable/unreliable message delivery and sequencing but zero confidentiality or integrity protection.
- Game state, player names, chat messages, and movement commands are all unencrypted.

### Protocol Attack Surface
- The network protocol (`protocol.h`) defines server-to-client and client-to-server message types as simple byte opcodes.
- **`svc_stufftext` (opcode 9)**: The server can send arbitrary console commands to the client for execution. This is an inherent remote code execution vector — a malicious server can force clients to execute any engine command. This is by design for gameplay (`reconnect`, `cmd spawn`, etc.) but represents a significant trust issue.
  ```c
  #define svc_stufftext 9  // [string] stuffed into client's console buffer
  ```
- **Buffer sizes**: `MAX_MSGLEN = 8000`, `MAX_DATAGRAM = 1024`, `NET_MAXMESSAGE = 8192`. Messages exceeding these sizes could cause issues, though `sizebuf_t` has an `overflowed` flag to detect this.

### No Rate Limiting
- No connection throttling or rate limiting exists. A flood of connection requests could overwhelm a server.
- The server sends entity updates every frame with no bandwidth management.

## Buffer Safety

### Pervasive Use of Unsafe C Functions

The codebase makes extensive use of buffer-unsafe functions throughout:

| Function | Used In | Risk |
|----------|---------|------|
| `sprintf` | `console.c`, `host.c`, `host_cmd.c`, `common.c`, `sv_main.c`, many more | No bounds checking — buffer overflow if format output exceeds buffer |
| `vsprintf` | `Con_Printf`, `Con_DPrintf`, `Con_SafePrintf`, `Con_DebugLog`, `Sys_Error`, `Host_EndGame` | Same — format string length not bounded |
| `strcpy` | Throughout | No bounds checking |
| `strcat` | Throughout | No bounds checking |
| `gets` | Not directly, but console input paths are similarly unbounded | — |

**Specific examples from the codebase**:

1. **`Con_DebugLog` in `console.c`**:
   ```c
   static char data[1024];
   va_start(argptr, fmt);
   vsprintf(data, fmt, argptr);  // No size limit
   ```
   A message longer than 1024 bytes would overflow the stack buffer.

2. **`Con_Printf` in `console.c`**:
   ```c
   #define MAXPRINTMSG 4096
   char msg[MAXPRINTMSG];
   vsprintf(msg, fmt, argptr);  // No snprintf
   ```
   Comment: `// FIXME: make a buffer size safe vsprintf?` — developers were aware of the issue.

3. **`Con_Init` in `console.c`**:
   ```c
   #define MAXGAMEDIRLEN 1000
   char temp[MAXGAMEDIRLEN+1];
   if (strlen(com_gamedir) < (MAXGAMEDIRLEN - strlen(t2)))
       sprintf(temp, "%s%s", com_gamedir, t2);
   ```
   This shows a manual bounds check pattern — the only type of overflow protection used.

4. **`Host_EndGame` in `host.c`**:
   ```c
   char string[1024];
   vsprintf(string, message, argptr);  // Could overflow
   ```

### Fixed-Size Buffers

Nearly all buffers are statically sized:
- `MAX_QPATH = 64` — game file paths
- `MAX_OSPATH = 128` — OS file paths  
- `MAXCMDLINE = 256` — console command line
- `MAX_SCOREBOARDNAME = 32` — player names
- `NET_NAMELEN = 64` — network addresses
- `CON_TEXTSIZE = 16384` — console text ring buffer

These are not validated consistently at input boundaries.

## File System Security

### Path Traversal
- File operations in `common.c` (`COM_FindFile`, `COM_LoadFile`) use raw string concatenation to build file paths from game directory and file names.
- No path traversal prevention exists — a crafted file name containing `../` could potentially access files outside the game directory.
- PAK file loading trusts the file entries within the archive without sanitization.

### File Permissions
- `Con_DebugLog` creates log files with mode `0666` (world-readable/writable):
  ```c
  fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
  ```
- No file creation permissions are restricted.

## Memory Safety

### No Memory Protection
- The custom memory allocator (`zone.c`) has minimal bounds checking:
  - Zone blocks have sentinel values (`ZONEID = 0x1d4a11`) checked in `Z_Free` and `Z_CheckHeap`
  - Hunk allocations are 16-byte aligned but not bounds-checked
  - Cache blocks can be freed at any time, creating potential use-after-free if references aren't cleared
- `Sys_MakeCodeWriteable()` in `sys.h` explicitly makes code pages writable for self-modifying assembly optimizations — disabling DEP/NX protections.

### No Stack Protection
- No stack canaries, ASLR, or DEP considerations (predates all of these technologies).

## Input Validation

### Console Command Injection
- Console input is parsed as tokenized text strings (`cmd.c`). Commands can be chained with `;` separator.
- The `stufftext` server message injects commands directly into the client's command buffer with no filtering.
- Key bindings can execute arbitrary commands.

### Entity Data
- Entity field values from QuakeC progs are not validated against ranges. Invalid model indices, sound indices, or entity references could cause crashes.
- `MAX_EDICTS = 600` is enforced but with a `// FIXME: ouch! ouch! ouch!` comment suggesting it's a known limitation.

## Privilege Model

- **Single privilege level**: All console commands are available to the local player.
- **Server commands**: `host_cmd.c` distinguishes `cmd_source == src_command` (local) vs `src_client` (remote player) and restricts some commands to local execution (e.g., `Host_Quit_f`).
- **Privileged clients**: The `client_s.privileged` field exists in `server.h` but is only used in the commented-out `IDGODS` system.
- **No access control** on server administration commands beyond being at the console.

## Known Vulnerability Patterns

| Category | Description | Location |
|----------|-------------|----------|
| Buffer overflow | `vsprintf` to fixed buffers | `console.c`, `host.c`, `host_cmd.c`, `sys_win.c` |
| Format string | `Con_Printf(msg)` patterns (mostly safe — `%s` used) | Various — actual format string bugs unlikely due to `"%s"` convention |
| Remote code execution | `svc_stufftext` allows server → client command injection | `cl_parse.c` |
| DoS | No connection rate limiting, no packet validation | `net_dgrm.c`, `net_main.c` |
| Information disclosure | All network traffic plaintext | `net_dgrm.c` |
| Path traversal | Unsanitized file paths from game content | `common.c` |
| Integer overflow | Many size calculations without overflow checks | `model.c`, `zone.c` |
| Memory corruption | Self-modifying code, no memory protections | Assembly files, `sys.h` |

## Summary

The codebase has **no security architecture** by modern standards. This is expected for a 1996 desktop game, but any modernization effort must consider:
1. Replacing all `sprintf`/`vsprintf` with bounded alternatives
2. Adding network protocol authentication and potentially encryption
3. Sanitizing file paths and limiting file system access
4. Adding input validation on all network-received data
5. Removing self-modifying code patterns
6. Implementing proper memory safety (bounds checking, use-after-free prevention)
