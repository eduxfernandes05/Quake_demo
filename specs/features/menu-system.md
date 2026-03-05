# Feature: Menu System

## Feature ID
`menu-system`

## Purpose
Provide a hierarchical graphical menu interface for game configuration, starting games, joining multiplayer sessions, and adjusting options without using the console. The menu system is the primary user interface for non-developer players.

## Scope
- Main menu with navigation to all sub-menus
- Single-player: new game, load, save
- Multiplayer: join, host, network setup
- Options: controls, video, audio settings
- Help screens
- Quit confirmation
- All rendering done via 2D drawing primitives

## User Workflows

### Starting a New Single-Player Game
1. Player presses Escape to open main menu
2. Selects "Single Player" → "New Game"
3. Selects difficulty level (Easy, Normal, Hard, Nightmare)
4. Engine executes `map start` → loads episode selection map
5. Player walks through episode portal in-game

### Loading a Saved Game
1. Main Menu → Single Player → Load Game
2. Menu displays 12 save slots with descriptions
3. Player selects a slot, game loads via `Host_Loadgame_f`
4. Restores full game state (entities, player stats, map)

### Saving a Game
1. Main Menu → Single Player → Save Game (only available during gameplay)
2. Menu displays 12 save slots
3. Player selects slot, game saves via `Host_Savegame_f`
4. Save file written to `<gamedir>/s<slot>.sav`

### Joining a Multiplayer Game
1. Main Menu → Multiplayer → Join a Game
2. Select network type: IPX, TCP/IP, Serial, Modem
3. For TCP/IP: Enter server address or search for LAN servers
4. For Serial/Modem: Configure COM port, baud rate
5. Connect to server, enter gameplay

### Hosting a Multiplayer Game
1. Main Menu → Multiplayer → New Game
2. Configure: max players (2-16), game type (DM/Coop), level, rules
3. Start game, server begins accepting connections

### Adjusting Options
1. Main Menu → Options
2. Adjust: mouse sensitivity, volume, music volume, screen size
3. Customize Controls → rebind keys for movement, shooting, etc.
4. Video Options → select resolution and mode

## Functional Requirements

### FR-MENU-01: Menu Screen Hierarchy
```
Main Menu
├── Single Player
│   ├── New Game (difficulty select)
│   ├── Load Game (12 slots)
│   └── Save Game (12 slots)
├── Multiplayer
│   ├── Join a Game
│   │   ├── IPX
│   │   ├── TCP/IP (address entry, LAN search)
│   │   ├── Serial Config
│   │   └── Modem Config
│   └── New Game (host)
│       └── Game Options (players, rules, level)
├── Options
│   ├── Key Bindings
│   └── Video Options
├── Help (6 pages)
└── Quit (confirmation prompt)
```

### FR-MENU-02: Menu State Machine
- State enum: `m_none`, `m_main`, `m_singleplayer`, `m_load`, `m_save`, `m_multiplayer`, `m_setup`, `m_net`, `m_options`, `m_video`, `m_keys`, `m_help`, `m_quit`, `m_serialconfig`, `m_modemconfig`, `m_lanconfig`, `m_gameoptions`, `m_search`, `m_slist`
- Each state has: Draw function, Key handler function, Menu entry function
- Escape returns to parent menu (or closes menu from main)

### FR-MENU-03: Menu Rendering
- Centered 320-pixel-wide layout (scaled for higher resolutions)
- Graphics loaded from `gfx/` directory in PAK files
- Menu items drawn as graphic images (not text)
- Cursor animation (spinning Quake logo or highlight bar)
- Enter sound on menu transitions (`m_entersound`)

### FR-MENU-04: Options Sliders
- Screen size slider: adjusts `scr_viewsize` (30-120)
- Brightness slider: adjusts `gamma`
- Mouse speed slider: adjusts `sensitivity`
- CD music volume: adjusts `bgmvolume`
- Sound volume: adjusts `volume`
- Always run toggle
- Invert mouse toggle
- Lookspring/lookstrafe toggles

### FR-MENU-05: Key Binding Interface
- Displays bindable actions with current key assignment
- Player selects action, then presses desired key
- Supports: attack, jump, forward, back, left, right, strafe left/right, look up/down, center view, mouselook, swim up/down, run

### FR-MENU-06: Multiplayer Setup
- Player name entry (with character editing)
- Player color selection (shirt/pants color, 14 colors each)
- Network configuration: IP address, port, hostname
- Game options: map selection, max players, game rules

### FR-MENU-07: Save/Load System Integration
- 12 save slots numbered 0-11
- Each slot shows description line from save file header
- Empty slots show "--- UNUSED SLOT ---"
- Quick save/load not implemented in menu (console only)

## Implementation Files

| File | Purpose |
|------|---------|
| `menu.c` | All menu screens: draw, key handling, state machine (~2000 lines) |
| `menu.h` | Menu interface declarations |
| `keys.c` | Key routing to menu state |
| `draw.c` | 2D drawing primitives used by menus |
| `gl_draw.c` | OpenGL 2D drawing for menus |
| `sbar.c` | Status bar (shares drawing code) |

## Configuration
No dedicated cvars for the menu system itself. The menu modifies other subsystem cvars:
- `scr_viewsize`, `sensitivity`, `volume`, `bgmvolume`, `gamma`
- `_cl_name`, `_cl_color` (player identity)
- `hostname`, `maxplayers`, `deathmatch`, `coop`, `fraglimit`, `timelimit`

## Dependencies
- **Keys** (`keys.c`): Input routing when `key_dest == key_menu`
- **Console** (`cmd.c, cvar.c`): Menu actions execute console commands
- **Draw** (`draw.c`): 2D graphics rendering for menu elements
- **Sound** (`snd_dma.c`): Menu sound effects (item select, slider)
- **Network** (`net_main.c`): Server search, connection initiation
- **Host** (`host_cmd.c`): Save/load game commands

## Acceptance Criteria
- AC-01: Menu opens on Escape and closes on Escape from main
- AC-02: All menu items are navigable with arrow keys and Enter
- AC-03: New game starts correctly on difficulty select
- AC-04: Save game writes to disk and load restores full state
- AC-05: Multiplayer join connects to specified server
- AC-06: Options sliders adjust cvars in real-time
- AC-07: Key binding interface captures and stores new bindings
- AC-08: Player name and color changes reflect in multiplayer
