# Feature: Demo Recording and Playback

## Feature ID
`demo-recording`

## Purpose
Record and play back gameplay sessions as demo files (`.dem`), capturing all network messages from the server's perspective. Used for entertainment (sharing gameplay), benchmarking (timedemo), and debugging.

## Scope
- Record gameplay to `.dem` files
- Play back recorded demos
- Time demo mode for benchmarking (plays back as fast as possible)
- Demo files intercept the network message layer

## User Workflows

### Recording a Demo
1. Player connects to a server (or starts single-player)
2. Player types `record <demoname>` in console
3. Engine opens `<gamedir>/<demoname>.dem` for writing
4. Every server message received by client is written to file (with timestamps and view angles)
5. Player types `stop` to end recording
6. File is closed and persisted

### Playing a Demo
1. Player types `playdemo <demoname>` in console
2. Engine disconnects from any server
3. Demo file opens, client state changes to playback mode
4. Messages are read from file instead of network
5. Game renders as if connected to a live server
6. Demo ends when file is exhausted → returns to console

### Benchmarking with Timedemo
1. Player types `timedemo <demoname>` in console
2. Demo plays back with no frame timing (renders every frame as fast as possible)
3. After completion, engine reports: total time, total frames, average FPS
4. Standard benchmark demo: `demo1.dem` (included with shareware Quake)

## Functional Requirements

### FR-DEMO-01: Recording Format
```
Message record (repeating):
  int32   message_length       // size of server message
  float   viewangles[3]        // player view pitch, yaw, roll
  byte[]  message_data         // raw server message bytes
```
- File starts with connection signon sequence (same as live join)
- All `svc_*` messages recorded verbatim
- View angles recorded for each message for spectator viewpoint

### FR-DEMO-02: Recording Lifecycle
- `CL_Record_f`: Validates state (must be connected), opens file, writes signon data
- `CL_WriteDemoMessage`: Called for each received server message
- `CL_Stop_f`: Closes demo file
- Recording transparent to gameplay (no performance impact on game logic)

### FR-DEMO-03: Playback Lifecycle
- `CL_PlayDemo_f`: Opens demo file, sets `cls.demoplayback = true`
- `CL_GetMessage`: During playback, reads from file instead of network
- Time-based gating: messages played back at original timing (based on `cl.time`)
- `CL_StopPlayback`: Called at EOF or `disconnect`
- During playback, no user input is sent (spectator mode)

### FR-DEMO-04: Timedemo Mode
- `CL_TimeDemo_f`: Starts playback with `cls.timedemo = true`
- Disables time gating: every frame reads the next message immediately
- Records start time and frame count
- `CL_FinishTimeDemo`: Calculates and prints results
- Output: `<frames> frames <seconds> seconds <fps> fps`

### FR-DEMO-05: Integration with Network Layer
- `CL_GetMessage` is the interception point
- During recording: after receiving real network message, calls `CL_WriteDemoMessage`
- During playback: replaces `NET_GetMessage` with file read
- All client-side processing works identically (demo is indistinguishable from live play)

## Implementation Files

| File | Purpose |
|------|---------|
| `cl_demo.c` | All demo functionality: record, playback, timedemo (~250 lines) |
| `cl_main.c` | `CL_Record_f`, `CL_Stop_f`, `CL_PlayDemo_f`, `CL_TimeDemo_f` command registration |
| `client.h` | `cls.demoplayback`, `cls.demorecording`, `cls.demofile`, `cls.timedemo` |
| `host.c` | Demo-aware frame timing adjustments |

## Console Commands
| Command | Description |
|---------|-------------|
| `record <name>` | Start recording to `<name>.dem` |
| `stop` | Stop recording |
| `playdemo <name>` | Play back a demo file |
| `timedemo <name>` | Benchmark playback (max speed) |

## Configuration
No dedicated cvars. Behavior controlled by:
- Game directory for file location
- Client connection state (must be connected to record)

## Dependencies
- **Client** (`cl_main.c`): Client state machine, message processing
- **Network** (`net_main.c`): Message interception layer
- **Filesystem** (`common.c`): File I/O for demo files
- **Host** (`host.c`): Frame timing (timedemo overrides)

## Acceptance Criteria
- AC-01: Recording captures all server messages without data loss
- AC-02: Playback reproduces identical visual output to live play
- AC-03: Timedemo reports accurate frame count and timing
- AC-04: Demo playback handles end-of-file gracefully (returns to console)
- AC-05: Recording does not impact gameplay performance
- AC-06: Demo files are portable across same-version clients
