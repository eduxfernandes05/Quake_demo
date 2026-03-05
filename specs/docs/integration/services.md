# External Service Dependencies

## Overview

WinQuake (1996–1997) has **no external service dependencies**. The engine operates as a fully self-contained application with no cloud services, third-party APIs, telemetry, update mechanisms, or online service integrations.

## Local OS Services

### CD Audio Subsystem

The only "external service" is the system CD-ROM drive for music playback:

| Platform | Implementation | API |
|----------|---------------|-----|
| Windows | `cd_win.c` | Win32 MCI (Media Control Interface) |
| Linux | `cd_linux.c` | Direct `/dev/cdrom` ioctl |
| Null | `cd_null.c` | Stub (no CD) |

**Operations**: `CDAudio_Init`, `CDAudio_Play`, `CDAudio_Stop`, `CDAudio_Pause`, `CDAudio_Resume`, `CDAudio_Update`

Console commands: `cd on/off/play/loop/stop/pause/resume/eject/info`

### Graphics Drivers

Platform-specific graphics APIs used (local, not networked):

| Driver | Files | API |
|--------|-------|-----|
| Windows DIB | `vid_win.c` | GDI `CreateDIBSection`, `BitBlt` |
| Windows MGL | `vid_win.c` | SciTech MGL (3rd-party library) |
| Linux SVGALib | `vid_svgalib.c` | `vga_setmode`, direct framebuffer |
| Linux X11 | `vid_x.c` | Xlib `XCreateImage`, shared memory |
| OpenGL (Windows) | `gl_vidnt.c` | WGL + OpenGL 1.1 |
| OpenGL (Linux) | `gl_vidlinuxglx.c` | GLX + OpenGL |
| 3Dfx Glide | via MGL or direct | 3Dfx Glide 2.x SDK |

### Sound Drivers

Local audio hardware interfaces:

| Platform | Implementation | API |
|----------|---------------|-----|
| Windows | `snd_win.c` | DirectSound (DSOUND) + legacy waveOut |
| Linux | `snd_linux.c` | `/dev/dsp` (OSS) |
| Null | `snd_null.c` | Stub (no sound) |

### Input Drivers

Local input device interfaces:

| Platform | Implementation | API |
|----------|---------------|-----|
| Windows | `in_win.c` | DirectInput + Win32 messages |
| Linux | `in_svgalib.c` | SVGALib keyboard/mouse |
| Linux X11 | `vid_x.c` | Xlib XGrabKeyboard/XGrabPointer |
| DOS | `dos_v2.c` | BIOS interrupts |

## Network Services (Peer-to-Peer)

Quake networking is **entirely peer-to-peer/self-hosted**. There are no central servers, matchmaking services, or authentication providers.

### Network Drivers

| Driver | Files | Transport |
|--------|-------|-----------|
| Loopback | `net_loop.c` | In-process (single player) |
| TCP/IP | `net_udp.c` (Unix), `net_wins.c` / `net_wipx.c` (Windows) | UDP/IP, IPX/SPX |
| Serial | `net_ser.c` | RS-232 serial, null modem |
| Modem | Subset of serial | Hayes AT modem commands |

### Master Server

The code contains a reference to a master server for server listing:

```c
// net_main.c
cvar_t  net_masterserver = {"net_masterserver", "192.246.40.37:27000"};
```

This was id Software's original master server (long offline). The protocol is a simple UDP query/response for server discovery. No authentication or session management.

## External Tool Dependencies

| Tool | Purpose | Required At |
|------|---------|------------|
| QBSP/VIS/LIGHT | Map compilation | Map authoring (not runtime) |
| QCCC | QuakeC compiler | Game logic authoring (not runtime) |
| TexMake | Texture WAD creation | Asset authoring (not runtime) |

These are separate executables, not integrated into the engine. Runtime has no external tool dependencies.

## Modernization Notes

For any modernization effort, the following external services could be introduced:
- **Authentication**: Steam, Epic, or OAuth2 for player identity
- **Matchmaking**: Cloud-based lobby/matchmaking service
- **Analytics**: Telemetry and crash reporting
- **Updates**: Auto-update mechanism (replacing manual patching)
- **Cloud saves**: Replace local save files with cloud storage
- **Voice chat**: Replace no-op with WebRTC or platform voice
- **Anti-cheat**: Server-side validation (currently trusting client)

The current architecture's complete lack of external dependencies makes it highly portable but provides no modern online service capabilities.
