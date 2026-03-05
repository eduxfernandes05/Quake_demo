# Feature: Input System

## Feature ID
`input-system`

## Purpose
Capture and route player input from keyboard, mouse, and joystick devices to the appropriate consumer (game movement, console text entry, or menu navigation). The input system provides platform-abstracted device access with configurable key bindings.

## Scope
- Keyboard input capture and key event dispatch
- Mouse input with configurable sensitivity and inversion
- Joystick/gamepad support (Windows)
- Input routing based on active mode (game, console, menu, chat)
- Key binding to console commands
- Mouse look and movement modes

## User Workflows

### Gameplay Input
1. Player presses movement keys (WASD or arrow keys by default)
2. Input system reads device state via platform driver
3. Key events dispatched through `Key_Event(key, down)`
4. If `key_dest == key_game`: bound commands are executed (e.g., `+forward`)
5. `+`/`-` commands set/clear movement flags in `in_*` variables
6. `CL_BaseMove` reads accumulated input into `usercmd_t`
7. Mouse deltas scaled by `sensitivity` cvar, added to view angles
8. Movement command sent to server as `clc_move`

### Console Input
1. Player presses `~` to open console (`key_dest` → `key_console`)
2. Keyboard input routed to console text entry
3. Printable characters added to command line buffer
4. Enter executes command, Up/Down navigate history
5. Tab triggers command completion

### Menu Input
1. Player presses Escape (`key_dest` → `key_menu`)
2. Arrow keys navigate menu items
3. Enter activates selection
4. Escape backs up or closes menu
5. Some menus accept text input (player name, server address)

### Chat Input
1. Player presses T or Y for team/all chat (`key_dest` → `key_message`)
2. Keyboard input routed to chat message buffer
3. Enter sends message via `say` or `say_team` command
4. Escape cancels message

## Functional Requirements

### FR-INPUT-01: Key Event System
- 256 key slots covering: keyboard, mouse buttons (3), joystick buttons (4), aux buttons (32)
- `Key_Event(int key, qboolean down)`: Central dispatch
- Key destination routing: `key_game`, `key_console`, `key_message`, `key_menu`
- Modifier tracking: shift state for uppercase/symbols
- Auto-repeat: `key_repeats[]` counts held-key repeats

### FR-INPUT-02: Mouse Input
- Raw mouse delta capture per frame
- Sensitivity scaling: `sensitivity` cvar (default varies)
- Invert Y-axis: `m_pitch` cvar (negative = inverted)
- Mouse look: `+mlook` command enables mouselook mode
- Mouse button binding: MOUSE1/MOUSE2/MOUSE3
- Exclusive mouse capture during gameplay (released in menu/console)

### FR-INPUT-03: Joystick Support
- Windows only (`in_win.c`): WinMM joystick API
- Analog axis mapping to movement and view
- Joy button binding (JOY1-JOY4)
- Configurable dead zone and sensitivity
- AUX buttons (AUX1-AUX32) for extended controllers

### FR-INPUT-04: Key Binding
- `bind <key> "<command>"`: Assign command string to key
- `unbind <key>`: Remove binding
- `unbindall`: Clear all bindings
- Protected keys: console toggle, escape cannot be rebound
- `+` prefix commands: press → `+cmd`, release → `-cmd`
- Movement commands: `+forward`, `+back`, `+moveleft`, `+moveright`, `+left`, `+right`, `+lookup`, `+lookdown`, `+jump`, `+attack`, `+speed`, `+strafe`, `+mlook`

### FR-INPUT-05: Movement Command Generation
- `CL_BaseMove`: Reads input state into `usercmd_t`
- Forward/back speed: `cl_forwardspeed` (default 200), `cl_backspeed` (200)
- Side speed: `cl_sidespeed` (350)
- Up speed: `cl_upspeed` (200)
- Always run: speeds doubled when `+speed` held (or `cl_forwardspeed` > 200)
- Lookspring: auto-center view when movement keys released
- Lookstrafe: mouse horizontal → side movement instead of turn

### FR-INPUT-06: Platform Abstraction
| Platform | Implementation | Devices |
|----------|---------------|---------|
| Windows | `in_win.c` | DirectInput mouse, WinMM joystick, Win32 keyboard |
| Linux SVGALib | `in_svgalib.c` | SVGALib keyboard/mouse |
| Linux X11 | `vid_x.c` (integrated) | Xlib keyboard/mouse |
| DOS | `dos_v2.c` | BIOS keyboard, PS/2 mouse |

## Implementation Files

| File | Purpose |
|------|---------|
| `keys.c` | Key event dispatch, binding system, console key handling |
| `keys.h` | Key constants (`K_TAB`, `K_ESCAPE`, `K_MOUSE1`, etc.) |
| `cl_input.c` | Movement command generation (`CL_BaseMove`), `+`/`-` commands |
| `in_win.c` | Windows DirectInput mouse + WinMM joystick |
| `in_svgalib.c` | Linux SVGALib input |
| `vid_x.c` | X11 input (integrated with video) |
| `dos_v2.c` | DOS input |
| `client.h` | `usercmd_t` command structure |

## Configuration (CVars)
| CVar | Default | Description |
|------|---------|-------------|
| `sensitivity` | 3 | Mouse sensitivity multiplier |
| `m_pitch` | 0.022 | Mouse pitch speed (negative = invert) |
| `m_yaw` | 0.022 | Mouse yaw speed |
| `m_forward` | 1 | Mouse forward speed |
| `m_side` | 0.8 | Mouse side speed |
| `cl_forwardspeed` | 200 | Forward movement speed |
| `cl_backspeed` | 200 | Backward movement speed |
| `cl_sidespeed` | 350 | Side movement speed |
| `cl_upspeed` | 200 | Swim/fly up speed |
| `cl_anglespeedkey` | 1.5 | Keyboard turn speed multiplier |
| `cl_movespeedkey` | 2.0 | Run speed multiplier |
| `lookspring` | 0 | Auto-center view on move |
| `lookstrafe` | 0 | Mouse horizontal → strafe |
| `m_filter` | 0 | Average mouse input over 2 frames |
| `joy_name` | "joystick" | Joystick device name |
| `joy_advanced` | 0 | Advanced joystick config |
| `joyadvaxisx/y/z/r/u/v` | 0 | Per-axis mapping |
| `in_joystick` | 0 | Enable joystick |

## Dependencies
- **Keys** (`keys.c`): Central dispatch for all key events
- **Client** (`cl_main.c`): Connection state affects input routing
- **Console** (`console.c`): Text entry target
- **Menu** (`menu.c`): Menu navigation target
- **Network** (`net_main.c`): Movement commands sent to server
- **Platform** (`sys_*.c`): Device initialization and events

## Acceptance Criteria
- AC-01: Keyboard, mouse, and joystick input captured correctly on all platforms
- AC-02: Key bindings persist in config.cfg and load on startup
- AC-03: Mouse sensitivity adjusts movement proportionally
- AC-04: Input correctly routes between game/console/menu/chat modes
- AC-05: `+`/`-` commands generate correct press/release behavior
- AC-06: Mouse is captured during gameplay and released in menus
- AC-07: Joystick dead zones prevent drift at center position
