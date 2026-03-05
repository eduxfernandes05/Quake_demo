# Feature: Console System

## Feature ID
`console-system`

## Purpose
Provide an interactive drop-down developer console for executing commands, modifying variables (cvars), viewing log output, and scripting via config files. The console is the primary control interface for engine configuration and debugging.

## Scope
- Drop-down overlay console with scrollback buffer
- Command parsing and execution
- Console variable (cvar) registration, get, set
- Key binding system (bind keys → commands)
- Alias system (named command sequences)
- Config file execution (`exec <file>`)
- Tab completion
- Command history navigation
- Log output from all engine subsystems

## User Workflows

### Interactive Console Usage
1. Player presses `~` (tilde) key to toggle console overlay
2. Console slides down over game view (partial or full screen)
3. Player types commands (e.g., `map e1m1`, `god`, `give 9 999`)
4. Engine parses, tokenizes, and dispatches command
5. Output/response printed to console scrollback
6. Player uses Up/Down arrows to navigate command history
7. Press `~` again to dismiss console

### Configuration via Config Files
1. Engine auto-executes `quake.rc` → `default.cfg` → `config.cfg` at startup
2. Config files contain command sequences: key bindings, cvar settings, aliases
3. Players can `exec myconfig.cfg` to load custom settings
4. `host_writeconfig` saves current bindings/cvars to `config.cfg`

### Key Binding
1. Player types `bind <key> <command>` in console
2. Key is mapped to command string in `keybindings[256]` array
3. When key is pressed during gameplay, bound command string is executed
4. Some keys are "console keys" that work only in console mode

### Alias Definitions
1. Player types `alias <name> "<command sequence>"`
2. Alias is stored as named command string
3. When alias name is typed/executed, its command string runs
4. Supports chaining: aliases can invoke other aliases/commands

## Functional Requirements

### FR-CON-01: Console Display
- Scrollback buffer: 32KB (`CON_TEXTSIZE`) of text
- Visible height configurable (half-screen or full-screen)
- Smooth slide animation for open/close
- Console background graphic with transparency
- Colored text (high-bit characters for alternate colors)
- 32 lines of command history

### FR-CON-02: Command System
- `Cmd_AddCommand(name, function)`: Register engine commands
- `Cmd_ExecuteString(text)`: Parse and execute a command string
- Tokenization via `Cmd_TokenizeString`: argc/argv style
- Command forwarding: unknown commands in `src_command` forward to server
- Stuffcmd: server can inject commands into client console

### FR-CON-03: Console Variables (CVars)
- `Cvar_RegisterVariable(var)`: Register a typed variable
- `Cvar_Set(name, value)`: Change variable value
- `Cvar_VariableValue(name)`: Get float value
- `Cvar_VariableString(name)`: Get string value
- Flags: `CVAR_ARCHIVE` (saved to config), `CVAR_SERVER` (notify server on change)
- `cvarlist` command shows all registered cvars

### FR-CON-04: Key Binding
- 256 key slots, each maps to a command string
- Console-only keys (can't be rebound): Tab, Enter, Escape, etc.
- Menu-bound keys: can't be rebound while in menu
- Shift key handling for uppercase/symbols
- Autorepeat support for held keys
- Key destination routing: `key_game`, `key_console`, `key_message`, `key_menu`

### FR-CON-05: Config File Execution
- `exec <filename>`: Load and execute command file from game directory
- Startup chain: `quake.rc` → `default.cfg` → `config.cfg` → `autoexec.cfg`
- `config.cfg` auto-saved on exit with current settings
- Config files are plain text with one command per line
- `+` prefix on command-line args executes as console commands

### FR-CON-06: Printing and Logging
- `Con_Printf(fmt, ...)`: Print to console and log file
- `Con_DPrintf(fmt, ...)`: Developer-only print (requires `developer 1`)
- `Con_SafePrintf(fmt, ...)`: Safe to call from any context
- `Con_DebugLog(file, fmt, ...)`: Write to debug log file
- Print output also goes to platform-specific console (stdout, Win32 console)
- `condebug` cvar enables persistent log file (`qconsole.log`)

## Implementation Files

| File | Purpose |
|------|---------|
| `console.c` | Console display, scrollback buffer, print functions |
| `console.h` | Console interface |
| `cmd.c` | Command registration, parsing, execution, aliases |
| `cmd.h` | Command system interface |
| `cvar.c` | Console variable registration and management |
| `cvar.h` | CVar types and interface |
| `keys.c` | Key binding, input routing, key name tables |
| `keys.h` | Key constants and interface |
| `zone.c` | Memory backing for command/cvar strings |

## Dependencies
- **Keys** (`keys.c`): Input routing to console/game/menu
- **Host** (`host.c`): Frame timing, `Host_Init` registers core commands
- **Filesystem** (`common.c`): Config file loading
- **Platform** (`sys_*.c`): Stdout/stderr output, platform console

## Acceptance Criteria
- AC-01: Console opens/closes smoothly on tilde keypress
- AC-02: Commands execute correctly from console input
- AC-03: CVars persist across sessions when CVAR_ARCHIVE is set
- AC-04: Key bindings save to and load from config.cfg
- AC-05: Command history navigates with Up/Down arrows (32 entries)
- AC-06: Config files execute all contained commands in order
- AC-07: Console scrollback retains 32KB of text without corruption
- AC-08: Developer prints only appear when `developer 1` is set
