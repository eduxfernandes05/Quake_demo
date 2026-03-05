# Feature: Mod and Content Support

## Feature ID
`mod-support`

## Purpose
Enable third-party modifications (mods) and custom content by providing a layered filesystem, replaceable game logic (QuakeC), and runtime-configurable game directories. This feature was groundbreaking in 1996 and established the modern game modding ecosystem.

## Scope
- Game directory system (`-game <moddir>`)
- Layered filesystem: base game (id1) + mod overlay
- Replaceable game logic via custom `progs.dat`
- Custom maps, models, sounds, textures
- PAK file support for content distribution
- Partial conversion and total conversion capability

## User Workflows

### Playing a Mod
1. Modder distributes mod as a directory (e.g., `ctf/`) containing custom content
2. Player places `ctf/` directory alongside `id1/` in Quake root
3. Player launches: `quake -game ctf`
4. Engine mounts `ctf/` directory as overlay on `id1/`
5. Files in `ctf/` override same-named files in `id1/`
6. Custom `progs.dat` replaces game logic entirely
7. Non-overridden assets fall through to `id1/`

### Creating a Mod
1. Modder writes QuakeC source (`.qc` files)
2. Compiles with QCC to produce `progs.dat`
3. Creates custom assets (maps `.bsp`, models `.mdl`, sounds `.wav`)
4. Packages in directory structure matching engine paths
5. Optionally bundles into PAK files for distribution

### Mission Pack Support
1. Official mission packs: `-hipnotic` (Scourge of Armagon), `-rogue` (Dissolution of Eternity)
2. Sets `com_gamedir` to mission pack directory
3. Engine enables pack-specific code paths (Hipnotic weapons in HUD, etc.)
4. Layering: `id1/` → `hipnotic/` or `rogue/`

## Functional Requirements

### FR-MOD-01: Game Directory System
- `-game <dirname>` command-line parameter
- Mounts specified directory as primary content source
- Fallback chain: `<gamedir>/` → `id1/` (always present)
- `com_gamedir` global stores active game directory path
- Save files, configs, demos stored in active game directory

### FR-MOD-02: Filesystem Layering
- File search order:
  1. `<gamedir>/` loose files
  2. `<gamedir>/pak0.pak`, `pak1.pak`, etc. (numbered)
  3. `id1/` loose files
  4. `id1/pak0.pak`, `pak1.pak`
- First match wins (mod content overrides base)
- Up to 10 search paths total

### FR-MOD-03: PAK Archive Support
- PAK files auto-detected and mounted from game directories
- Files inside PAK accessed transparently by name
- Multiple PAK files loaded in numeric order (pak0, pak1, ...)
- PAK files are read-only (writes go to loose files)
- Distribution: single PAK more efficient than loose files

### FR-MOD-04: Asset Replacement
All asset types replaceable by mod:
| Asset Type | Extension | Location |
|-----------|-----------|----------|
| Maps | `.bsp` | `maps/` |
| Models | `.mdl` | `progs/` |
| Sprites | `.spr` | `progs/` |
| Sounds | `.wav` | `sound/` |
| Demo files | `.dem` | root |
| Config files | `.cfg` | root |
| Textures | (in BSP) | `maps/` |
| Console pics | `.lmp` | `gfx/` |
| Game logic | `progs.dat` | root |

### FR-MOD-05: QuakeC Game Logic Replacement
- Custom `progs.dat` completely replaces game behavior
- All player mechanics, weapons, monsters, items can be changed
- New entity types defined via new spawn functions
- Game rules (deathmatch variants) fully programmable
- Server-side only: clients need no mod files for multiplayer

### FR-MOD-06: Precache System
- `precache_model()`, `precache_sound()`: called during map spawn
- Server notifies clients which assets to load
- Clients download/locate assets from their local filesystem
- No automatic download (assets must be pre-distributed)

## Implementation Files

| File | Purpose |
|------|---------|
| `common.c` | Filesystem: `COM_FindFile`, `COM_LoadPackFile`, `COM_AddGameDirectory`, search path management |
| `common.h` | Filesystem interface |
| `quakedef.h` | `MAX_SEARCHPATHS`, `GAMENAME` ("id1") |
| `pr_edict.c` | Progs loading: `PR_LoadProgs` |
| `sv_main.c` | Model/sound precaching for clients |
| `host.c` | `-game` parameter processing during `Host_Init` |
| `host_cmd.c` | Map loading, changelevel |
| `cl_parse.c` | Client-side asset loading from precache lists |

## Configuration
| Parameter | Description |
|-----------|-------------|
| `-game <dir>` | Set mod directory (command line) |
| `-hipnotic` | Enable Hipnotic mission pack mode |
| `-rogue` | Enable Rogue mission pack mode |
| `gamedir <dir>` | Change game directory at runtime (console) |

## Dependencies
- **Filesystem** (`common.c`): Core search path and PAK management
- **QuakeC VM** (`pr_exec.c`, `pr_edict.c`): Game logic loading
- **Model Loader** (`model.c`): Asset loading follows search paths
- **Sound** (`snd_mem.c`): Sound loading follows search paths
- **Server** (`sv_main.c`): Precache system coordinates asset distribution

## Acceptance Criteria
- AC-01: `-game <moddir>` loads mod content instead of base game
- AC-02: Mod assets override base assets with same names
- AC-03: Non-overridden assets fall through to id1 correctly
- AC-04: Custom `progs.dat` replaces all game logic
- AC-05: PAK files load transparently alongside loose files
- AC-06: Save/load works correctly in mod directories
- AC-07: Multiple mods can be tested without reinstalling (just change `-game`)
- AC-08: Mission pack flags enable pack-specific engine features
