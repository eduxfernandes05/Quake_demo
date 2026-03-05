# Feature: Networking and Multiplayer

## Feature ID
`networking-multiplayer`

## Purpose
Enable real-time multiplayer gameplay over LAN (IPX/TCP-IP), Internet (TCP/IP), serial cable, and modem connections using a client-server architecture. Even single-player uses a local loopback server.

## Scope
- Client-server networking model with authoritative server
- Multiple transport layers: loopback, UDP/IP, IPX/SPX, serial, modem
- Connection management: handshake, keepalive, disconnect
- Reliable and unreliable message channels
- Entity state replication with delta compression
- Server browser and master server query
- Up to 16 simultaneous players
- Dedicated server mode (headless)

## User Workflows

### Single-Player (Loopback)
1. Player starts new game from menu or `map` console command
2. Engine spawns local server (`SV_SpawnServer`)
3. Client connects via loopback driver (`net_loop.c`)
4. Full client-server protocol runs in-process
5. No network traffic; messages passed via shared memory buffer

### LAN Multiplayer
1. Host starts "New Game" from multiplayer menu, configures settings
2. Server binds to UDP port 26000 (default) or IPX socket
3. Other players use "Join Game" → "Search for local games"
4. Server responds to broadcast queries with server info
5. Client selects server, sends connection request
6. Server accepts, allocates client slot, sends spawn parameters
7. Gameplay proceeds with real-time state sync

### Internet Multiplayer
1. Player enters server IP address in "Join Game" → "TCP/IP" menu
2. Client sends connection request to specified IP:port
3. Same connection flow as LAN after initial contact
4. Master server at `192.246.40.37:27000` for server listing (historical)

### Serial/Modem
1. One player selects "Direct connect" or "Modem" from multiplayer menu
2. Configures COM port, baud rate, modem init strings
3. Serial driver establishes point-to-point link
4. Protocol runs over serial with CSLIP-like framing

### Dedicated Server
1. Launch with `-dedicated [maxplayers]` command line
2. No video, sound, or input initialization
3. Console I/O via stdin/stdout
4. Accepts client connections and runs game logic

## Functional Requirements

### FR-NET-01: Transport Layer Abstraction
- `net_landriver_t` vtable for transport drivers: Init, Listen, OpenSocket, CloseSocket, Connect, CheckNewConnections, Read, Write, Broadcast, AddrToString, GetSocketAddr, GetNameFromAddr, GetAddrFromName, GetDefaultMTU
- `net_driver_t` vtable for connection drivers: Init, Listen, SearchForHosts, Connect, CheckNewConnections, QGetMessage, QSendMessage, SendUnreliableMessage, CanSendMessage, CanSendUnreliableMessage, Close, Shutdown
- Runtime driver selection based on availability and user config

### FR-NET-02: Connection Protocol
- 3-way handshake: CCREQ_CONNECT → CCREP_ACCEPT → client starts signon
- Connection carries session identifier and protocol version check
- `CCREQ_CONNECT` includes game name ("QUAKE") and net protocol version (3)
- `CCREP_ACCEPT` returns assigned port for dedicated socket
- `CCREP_REJECT` returns human-readable reason string

### FR-NET-03: Message Channels
- **Reliable**: Sequenced, acknowledged, retransmitted on loss. Stop-and-wait protocol (one outstanding message at a time). Used for: entity spawn, level change, player info updates.
- **Unreliable**: Fire-and-forget datagrams. Used for: player movement, entity position updates, particle effects, sounds.
- Maximum datagram size: 1024 bytes (`MAX_DATAGRAM`)
- Maximum message size: 8192 bytes (`MAX_MSGLEN`)

### FR-NET-04: Entity State Replication
- Server sends full entity state on spawn (baseline)
- Subsequent updates use flag-based delta encoding (`U_ORIGIN1`, `U_ANGLE2`, `U_FRAME`, etc.)
- Entity update flags (from `protocol.h`):
  - `U_MOREBITS` — additional flag byte follows
  - `U_ORIGIN1/2/3` — position changed
  - `U_ANGLE1/2/3` — rotation changed
  - `U_FRAME` — animation frame
  - `U_COLORMAP` — player color
  - `U_SKIN` — skin index
  - `U_EFFECTS` — visual effects flags
  - `U_MODEL` — model changed
- Client interpolates between received states

### FR-NET-05: Client Input Processing
- Client sends `clc_move` with: pitch, yaw, roll, forward speed, side speed, up speed, button bits, impulse command
- Server processes movement authoritatively via `SV_ReadClientMove`
- Client-side prediction not implemented (pure server authority)

### FR-NET-06: Server Browser
- `CCREQ_SERVER_INFO` broadcast query
- Servers respond with `CCREP_SERVER_INFO` (name, map, current/max players)
- Master server: `CCREQ_RULE_INFO` for querying server rules
- Results displayed in server list menu

### FR-NET-07: Dedicated Server
- No video/sound/input subsystems initialized
- Console runs on stdin/stdout via `sys_dedicated.c` (Unix) or `conproc.c` (Windows)
- Suppresses all client-side code paths
- `isDedicated` global flag controls branching

## Implementation Files

### Core Networking
| File | Purpose |
|------|---------|
| `net_main.c` | Network subsystem coordinator, driver dispatching |
| `net_loop.c` | Loopback driver (single-player) |
| `net_dgrm.c` | Datagram connection protocol (reliable/unreliable) |
| `net.h` | Network types, vtables, protocol constants |

### Transport Drivers
| File | Purpose |
|------|---------|
| `net_udp.c` | Unix UDP/IP socket driver |
| `net_wins.c` | Windows UDP/IP (Winsock) |
| `net_wipx.c` | Windows IPX/SPX |
| `net_bsd.c` | BSD socket abstraction |
| `net_ser.c` | Serial port driver |
| `net_comx.c` | Low-level COM port access |
| `net_vcr.c` | Network recording/playback (debugging) |

### Server
| File | Purpose |
|------|---------|
| `sv_main.c` | Server main loop, client management |
| `sv_phys.c` | Server-side physics simulation |
| `sv_move.c` | Monster/NPC movement |
| `sv_user.c` | Player movement processing |
| `server.h` | Server data structures |

### Client
| File | Purpose |
|------|---------|
| `cl_main.c` | Client connection management, state machine |
| `cl_parse.c` | Server message parsing (`svc_*` handlers) |
| `cl_input.c` | Client input sampling and packaging |
| `cl_tent.c` | Temporary entities (projectiles, gibs) |
| `client.h` | Client data structures |

### Protocol
| File | Purpose |
|------|---------|
| `protocol.h` | Protocol constants, message types, entity flags |
| `common.c` | Message read/write primitives (MSG_Write*, MSG_Read*) |

## Configuration (CVars)
| CVar | Default | Description |
|------|---------|-------------|
| `hostname` | "UNNAMED" | Server name |
| `net_messagetimeout` | 300 | Connection timeout (seconds) |
| `net_masterserver` | "192.246.40.37:27000" | Master server address |
| `rcon_password` | "" | Remote console password |
| `sv_maxvelocity` | 2000 | Maximum entity velocity |
| `sv_gravity` | 800 | Gravity acceleration |
| `sv_friction` | 4 | Ground friction |
| `sv_maxspeed` | 320 | Maximum player speed |
| `sv_accelerate` | 10 | Player acceleration |
| `sv_aim` | 0.93 | Auto-aim angle |
| `teamplay` | 0 | Team play mode |
| `deathmatch` | 0 | Deathmatch mode |
| `coop` | 0 | Cooperative mode |
| `fraglimit` | 0 | Frag limit (0=none) |
| `timelimit` | 0 | Time limit (0=none) |

## Dependencies
- **Host** (`host.c`): Frame-level coordination, `Host_Frame` calls `SV_Frame` and `CL_Frame`
- **QuakeC VM** (`pr_exec.c`): Game logic callbacks (PlayerPreThink, PlayerPostThink, etc.)
- **Console** (`cmd.c`): Commands `connect`, `disconnect`, `reconnect`, `map`, etc.
- **Platform** (`sys_*.c`): Socket abstractions, timing

## Acceptance Criteria
- AC-01: Single-player game runs via loopback with zero packet loss
- AC-02: LAN multiplayer supports 16 players with < 100ms latency penalty
- AC-03: Reliable messages are delivered in-order without loss
- AC-04: Entity positions update smoothly on clients
- AC-05: Server browser discovers LAN servers within 3 seconds
- AC-06: Connection handshake completes within 5 seconds on LAN
- AC-07: Dedicated server runs without video/input subsystems
- AC-08: Connection gracefully handles timeout after 300 seconds idle
