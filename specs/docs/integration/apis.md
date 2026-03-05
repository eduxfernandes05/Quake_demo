# API Documentation — Network Protocol

## Overview

WinQuake does not expose REST, GraphQL, or WebSocket APIs. The "API" is the **Quake network protocol** — a custom binary UDP-based protocol for client-server communication. There is also a **console command API** for scripting/configuration and a **QuakeC built-in function API** for game logic.

## Network Protocol

### Transport Layer

- **Protocol**: UDP (connectionless datagrams)
- **Connection protocol version**: `NET_PROTOCOL_VERSION = 3` (`net.h`)
- **Game protocol version**: `PROTOCOL_VERSION = 15` (`protocol.h`)
- **Maximum message size**: `NET_MAXMESSAGE = 8192` bytes
- **Maximum datagram size**: `MAX_DATAGRAM = 1024` bytes
- **Maximum reliable message**: `MAX_MSGLEN = 8000` bytes

### Connection Handshake

Defined in `net.h`, the connection protocol:

#### Client → Server Requests
| Code | Type | Payload |
|------|------|---------|
| `0x01` | `CCREQ_CONNECT` | string `game_name` ("QUAKE") + byte `net_protocol_version` (3) |
| `0x02` | `CCREQ_SERVER_INFO` | string `game_name` + byte `net_protocol_version` |
| `0x03` | `CCREQ_PLAYER_INFO` | byte `player_number` |
| `0x04` | `CCREQ_RULE_INFO` | string `rule_name` |

#### Server → Client Responses
| Code | Type | Payload |
|------|------|---------|
| `0x81` | `CCREP_ACCEPT` | long `port` |
| `0x82` | `CCREP_REJECT` | string `reason` |
| `0x83` | `CCREP_SERVER_INFO` | string `server_address` + string `host_name` + string `level_name` + byte `current_players` + byte `max_players` + byte `protocol_version` |
| `0x84` | `CCREP_PLAYER_INFO` | byte `player_number` + string `name` + long `colors` + long `frags` + long `connect_time` + string `address` |
| `0x85` | `CCREP_RULE_INFO` | string `rule` + string `value` |

### Datagram Header (`net.h`)

```
┌────────────────┬────────────────┐
│  Flags + Len   │  Sequence Num  │
│  (4 bytes)     │  (4 bytes)     │
└────────────────┴────────────────┘
```

Flags (upper 16 bits):
- `NETFLAG_DATA (0x00010000)` — data packet
- `NETFLAG_ACK (0x00020000)` — acknowledgment
- `NETFLAG_NAK (0x00040000)` — negative acknowledgment
- `NETFLAG_EOM (0x00080000)` — end of message
- `NETFLAG_UNRELIABLE (0x00100000)` — unreliable packet
- `NETFLAG_CTL (0x80000000)` — control packet (connection/info)

### Server → Client Messages (`protocol.h`)

| Opcode | Name | Payload | Description |
|--------|------|---------|-------------|
| 0 | `svc_bad` | — | Error |
| 1 | `svc_nop` | — | No operation |
| 2 | `svc_disconnect` | — | Disconnect client |
| 3 | `svc_updatestat` | byte stat + long value | Update player stat |
| 4 | `svc_version` | long version | Server version |
| 5 | `svc_setview` | short entity | Set view entity |
| 6 | `svc_sound` | (complex) | Play sound |
| 7 | `svc_time` | float time | Server time |
| 8 | `svc_print` | string text | Print to console |
| 9 | `svc_stufftext` | string command | Execute console command on client |
| 10 | `svc_setangle` | 3×angle | Set view angles |
| 11 | `svc_serverinfo` | long+strings | Server info (map, models, sounds) |
| 12 | `svc_lightstyle` | byte+string | Set light style |
| 13 | `svc_updatename` | byte+string | Update player name |
| 14 | `svc_updatefrags` | byte+short | Update player frags |
| 15 | `svc_clientdata` | (complex) | Client state update |
| 16 | `svc_stopsound` | short | Stop a sound |
| 17 | `svc_updatecolors` | byte+byte | Update player colors |
| 18 | `svc_particle` | coords+dir+count+color | Particle effect |
| 19 | `svc_damage` | (complex) | Damage indicator |
| 20 | `svc_spawnstatic` | (complex) | Spawn static entity |
| 22 | `svc_spawnbaseline` | (complex) | Entity baseline |
| 23 | `svc_temp_entity` | byte type + data | Temporary entity (explosion, etc.) |
| 24 | `svc_setpause` | byte | Set pause state |
| 25 | `svc_signonnum` | byte | Signon progress |
| 26 | `svc_centerprint` | string | Center-screen message |
| 27 | `svc_killedmonster` | — | Increment monster kill count |
| 28 | `svc_foundsecret` | — | Increment secret count |
| 29 | `svc_spawnstaticsound` | (complex) | Ambient sound |
| 30 | `svc_intermission` | — | Intermission screen |
| 31 | `svc_finale` | string text | Finale text |
| 32 | `svc_cdtrack` | byte+byte | CD music track |
| 33 | `svc_sellscreen` | — | Show Quake registration screen |
| 34 | `svc_cutscene` | string text | Cutscene text |

### Entity Updates (Fast Updates)

When high bit of server message byte is set, it's a fast entity update. Flags determine which fields follow:

| Flag | Bit | Field | Size |
|------|-----|-------|------|
| `U_MOREBITS` | 0 | Read additional byte of flags | — |
| `U_ORIGIN1` | 1 | X origin | short (×8) |
| `U_ORIGIN2` | 2 | Y origin | short (×8) |
| `U_ORIGIN3` | 3 | Z origin | short (×8) |
| `U_ANGLE2` | 4 | Pitch angle | byte |
| `U_NOLERP` | 5 | No interpolation | — |
| `U_FRAME` | 6 | Animation frame | byte |
| `U_ANGLE1` | 8 | Yaw angle | byte |
| `U_ANGLE3` | 9 | Roll angle | byte |
| `U_MODEL` | 10 | Model index | byte |
| `U_COLORMAP` | 11 | Color map | byte |
| `U_SKIN` | 12 | Skin number | byte |
| `U_EFFECTS` | 13 | Effects flags | byte |
| `U_LONGENTITY` | 14 | Entity number is short (not byte) | — |

### Client → Server Messages (`protocol.h`)

| Opcode | Name | Payload | Description |
|--------|------|---------|-------------|
| 1 | `clc_nop` | — | No operation |
| 2 | `clc_disconnect` | — | Client disconnect |
| 3 | `clc_move` | 3×angles + 3×movement + button + impulse | Movement command |
| 5 | `clc_stringcmd` | string | Console command to server |

## Console Command API

Commands registered via `Cmd_AddCommand()` in various `*_Init()` functions:

### Host Commands (`host_cmd.c`)
`status`, `quit`, `god`, `fly`, `noclip`, `notarget`, `map`, `changelevel`, `connect`, `reconnect`, `name`, `color`, `kill`, `pause`, `spawn`, `begin`, `prespawn`, `kick`, `ping`, `save`, `load`, `give`, `startdemos`, `demos`, `stopdemo`, `viewmodel`, `viewframe`, `viewnext`, `viewprev`

### Console Commands (`console.c`)
`toggleconsole`, `messagemode`, `messagemode2`, `clear`

### Client Commands (`cl_main.c`, `cl_demo.c`)
`entities`, `disconnect`, `record`, `stop`, `playdemo`, `timedemo`

### Network Commands (`net_main.c`)
`slist`, `listen`, `maxplayers`, `port`, `net_stats`

## QuakeC Built-in Functions API

Defined in `pr_cmds.c`, these C functions are callable from QuakeC game scripts:

| # | Function | Description |
|---|----------|-------------|
| 1 | `makevectors(angles)` | Convert angles to forward/right/up vectors |
| 2 | `setorigin(entity, origin)` | Set entity position |
| 3 | `setmodel(entity, model)` | Set entity model |
| 4 | `setsize(entity, mins, maxs)` | Set entity bounding box |
| 6 | `break()` | Debug breakpoint |
| 7 | `random()` | Random float 0–1 |
| 8 | `sound(entity, channel, sample, vol, attenuation)` | Play sound |
| 9 | `normalize(vector)` | Normalize vector |
| 10 | `error(string)` | Fatal error from QC |
| 11 | `objerror(string)` | Object error (removes entity) |
| 12 | `vlen(vector)` | Vector length |
| 13 | `vectoyaw(vector)` | Vector to yaw angle |
| 14 | `spawn()` | Create new entity |
| 15 | `remove(entity)` | Remove entity |
| 16 | `traceline(start, end, nomonsters, passentity)` | Ray trace |
| 17 | `checkclient()` | Find client for monster AI |
| 18 | `find(start, field, string)` | Find entity by field value |
| 19 | `precache_sound(string)` | Precache a sound file |
| 20 | `precache_model(string)` | Precache a model file |
| 21 | `stuffcmd(client, string)` | Send console command to client |
| 22 | `findradius(origin, radius)` | Find entities in radius |
| 23 | `bprint(string)` | Broadcast print |
| 24 | `sprint(client, string)` | Print to specific client |
| 25 | `dprint(string)` | Developer print |
| 26 | `ftos(float)` | Float to string |
| 27 | `vtos(vector)` | Vector to string |
| 31 | `setspawnparms(entity)` | Set spawn parameters |
| 34 | `droptofloor()` | Drop entity to floor |
| 35 | `lightstyle(style, string)` | Set light animation |
| 40 | `checkbottom(entity)` | Check if entity is on ground |
| 41 | `pointcontents(point)` | Get contents at point |
| 43 | `fabs(float)` | Absolute value |
| 44 | `aim(entity, speed)` | Auto-aim calculation |
| 45 | `cvar(string)` | Read cvar value |
| 46 | `localcmd(string)` | Execute console command |
| 47 | `nextent(entity)` | Next entity in list |
| 48 | `particle(origin, dir, color, count)` | Particle effect |
| 49 | `ChangeYaw()` | Rotate entity toward ideal yaw |
| 52 | `WriteByte/Char/Short/Long/Coord/Angle/String` | Write to network message |
| 67 | `movetogoal(dist)` | Move entity toward goal |
| 68 | `precache_file(string)` | Precache a file |
| 69 | `makestatic(entity)` | Convert entity to static |
| 70 | `changelevel(string)` | Change map |
| 72 | `cvar_set(name, value)` | Set cvar value |
| 73 | `centerprint(client, string)` | Center-screen print |
| 74 | `ambientsound(pos, sample, vol, atten)` | Ambient sound |
| 75 | `precache_model2(string)` | Precache model (high index) |
| 76 | `precache_sound2(string)` | Precache sound (high index) |
| 77 | `precache_file2(string)` | Precache file (high index) |
