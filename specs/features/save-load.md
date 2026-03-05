# Feature: Save/Load Game

## Feature ID
`save-load`

## Purpose
Allow players to save their single-player game progress to disk and restore it later, preserving the complete game state including entity positions, player stats, inventory, and map progress.

## Scope
- 12 numbered save slots (s0.sav through s11.sav)
- Complete game state serialization (all entities)
- Player spawn parameters for cross-level persistence
- Autosave on level transitions (via spawn parameters)
- Text-based save file format

## User Workflows

### Saving a Game
1. Player opens menu → Single Player → Save Game
2. Menu shows 12 slots with save descriptions (or "--- UNUSED SLOT ---")
3. Player selects a slot
4. `Host_Savegame_f` serializes game state to `<gamedir>/s<slot>.sav`
5. Confirmation: console prints "Saving game to s<slot>.sav..."

### Loading a Game
1. Player opens menu → Single Player → Load Game (or uses `load` console command)
2. Menu shows 12 slots with save descriptions
3. Player selects a slot
4. `Host_Loadgame_f` reads save file, spawns server, restores all entities
5. Game resumes from exact saved state

### Console Commands
- `save <name>`: Save to named file
- `load <name>`: Load from named file
- `save s0` through `save s11`: Menu-compatible slots

### Level Transition Persistence
1. Player exits level through trigger_changelevel
2. Engine calls `Host_Changelevel_f` → saves spawn parameters
3. New level loads, player spawns with carried-over stats
4. 16 spawn parameters store persistent player data (health, weapons, ammo, keys, etc.)

## Functional Requirements

### FR-SAVE-01: Save File Format
```
<version>\n                      // Save format version (5)
<description>\n                  // Auto-generated: "<mapname> kills:<x>/<y>"
<spawn_parms[0..15]>\n          // 16 float values (player carry-over state)
<current_skill>\n                // Difficulty level (0-3)
<mapname>\n                      // Current map name
<time>\n                         // Server time
<lightstyles[0..63]>\n          // 64 light animation patterns
{                                // Entity blocks (one per entity)
"<key>" "<value>"\n             // Key-value pairs
}
```

### FR-SAVE-02: Entity Serialization
- All non-free edicts are serialized
- Free edicts written as empty blocks `{}`
- Entity fields written as key-value pairs from QuakeC field definitions
- Entity index preserved (array position matches edict number)
- Serverflags (episode completion) preserved as `spawn_parms[0]`

### FR-SAVE-03: State Restoration
- Server spawns fresh map via `SV_SpawnServer`
- Save time restored (`sv.time`)
- Light styles restored
- Each entity block: spawn fresh edict, apply key-value pairs via `ED_ParseEdict`
- Entity `_precache_*` fields re-link to precached resources
- Player entity triggers `RestoreGame` callback in QuakeC (if defined)

### FR-SAVE-04: Validation
- Save rejected if: no server active, not single-player, client not alive, `sv.state != ss_active`
- Load rejected if: file doesn't exist, version mismatch
- No file locking or corruption detection beyond format parsing

### FR-SAVE-05: Spawn Parameters
- 16 float values stored in `svs.clients[0].spawn_parms[]`
- Set by QuakeC `SetChangeParms()` before level transitions
- Restored by QuakeC `SetNewParms()` on new game only
- Carry: health, current weapon, ammo counts, items bitmask, armor, keys

## Implementation Files

| File | Purpose |
|------|---------|
| `host_cmd.c` | `Host_Savegame_f`, `Host_Loadgame_f` (~200 lines total) |
| `pr_edict.c` | `ED_Write`, `ED_ParseEdict` — entity serialization |
| `sv_main.c` | `SV_SpawnServer` — fresh server for load |
| `pr_cmds.c` | `SetChangeParms`, `SetNewParms` QuakeC callbacks |
| `menu.c` | Save/Load menu screens (12-slot UI) |
| `server.h` | `spawn_parms[]` in `client_t` |

## Configuration
No dedicated cvars. Save behavior controlled by:
- Game directory (id1, hipnotic, rogue)
- Save slot number (0-11)
- `deathmatch` cvar (save disabled if != 0)

## Dependencies
- **Host** (`host.c`): Central coordinator for save/load flow
- **QuakeC VM** (`pr_edict.c`): Entity serialization/deserialization
- **Server** (`sv_main.c`): Server spawn for load, entity allocation
- **Filesystem** (`common.c`): File read/write in game directory
- **Menu** (`menu.c`): Save/load slot selection UI

## Acceptance Criteria
- AC-01: Save captures all entity positions, health, ammo, and items
- AC-02: Load restores exact game state including player position and inventory
- AC-03: Level transition preserves player stats via spawn parameters
- AC-04: Save files are text-readable and parseable
- AC-05: 12 save slots maintain independent save states
- AC-06: Save is blocked in multiplayer/deathmatch modes
- AC-07: Save description shows map name and kill count
