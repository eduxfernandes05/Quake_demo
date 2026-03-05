# Feature: Asset Loading System

## Feature ID
`asset-loading`

## Purpose
Load, parse, and manage all game assets (maps, models, sprites, sounds, textures) from the layered filesystem (PAK archives and loose files). The asset system handles format parsing, memory allocation, and precache management for all game content.

## Scope
- BSP map loading and data structure construction
- Alias model (MDL) loading with skins and animation frames
- Sprite (SPR) loading with frame groups
- WAV sound file loading and resampling
- WAD2 graphics archive loading (console font, UI pics)
- Texture and lightmap data extraction
- Precache system for server-coordinated asset loading

## User Workflows

### Level Load Sequence
1. `Host_Map_f` or `Host_Changelevel_f` initiates level load
2. Server spawns: `SV_SpawnServer(mapname)`
3. `Mod_ForName(maps/<name>.bsp)` loads BSP world model
4. BSP lumps parsed: vertices, edges, faces, nodes, leaves, planes, textures, lightmaps, visibility, clip nodes, entities
5. Submodels (doors, platforms) extracted as separate models
6. Entity string parsed → QuakeC spawn functions execute
7. Spawn functions call `precache_model`, `precache_sound` → triggers loading
8. Client receives precache lists → loads models and sounds locally
9. Surface cache initialized for renderer
10. Level ready for gameplay

### Model Loading
1. `Mod_ForName(name)` called with model path
2. File loaded from filesystem (PAK or loose)
3. Format detected by magic number: "IDPO" (MDL), "IDSP" (SPR), or BSP version
4. Appropriate loader invoked: `Mod_LoadBrushModel`, `Mod_LoadAliasModel`, `Mod_LoadSpriteModel`
5. Data structures built in Hunk memory
6. Model cached for reuse (subsequent requests return cached pointer)

### Sound Loading
1. `S_PrecacheSound(name)` called during level init
2. `S_LoadSound(sfx)` triggered on first playback or precache
3. WAV file loaded from filesystem
4. Parsed for format, sample rate, bit depth
5. Resampled to output device rate if needed
6. Stored in Cache allocator (purgeable under memory pressure)

## Functional Requirements

### FR-ASSET-01: BSP Map Loading (`Mod_LoadBrushModel`)
Lumps loaded in order:
1. Vertex positions (`mvertex_t`)
2. Edge pairs (`medge_t`)
3. Surface edges (ordered edge index list)
4. Textures (mipmap chains, animation sequences)
5. Lightmaps (per-surface light data)
6. Planes (clipping/splitting planes)
7. Texture info (UV mapping vectors)
8. Faces (polygon surface descriptions)
9. Mark surfaces (leaf → face mapping)
10. Visibility (PVS compressed bitfield)
11. Leaves (BSP tree leaves with contents, ambient levels)
12. Nodes (BSP tree internal nodes)
13. Clip nodes (collision hulls — 3 sizes)
14. Entities (text string, parsed later)
15. Submodels (inline BSP models for movers)

### FR-ASSET-02: Alias Model Loading (`Mod_LoadAliasModel`)
- Parse MDL header (version 6)
- Load skins: single or grouped (animated skins)
- Load texture coordinates (`stvert_t`)
- Load triangles (3 vertex indices + front/back flag)
- Load animation frames: single or grouped (morph animation)
- Build vertex normal table for lighting
- All data stored contiguously in Hunk

### FR-ASSET-03: Sprite Loading (`Mod_LoadSpriteModel`)
- Parse SPR header (version 1)
- Load frame data: origin offset + pixel data
- Support frame groups (animated sprites)
- Orientation types: parallel (billboard), oriented, parallel-oriented

### FR-ASSET-04: WAV Sound Loading (`S_LoadSound`)
- Parse RIFF/WAV header
- Extract PCM data (8-bit or 16-bit)
- Resample to output device sample rate
- Optionally convert to 8-bit (`loadas8bit`)
- Store in Cache with `sfxcache_t` header
- Loop points detected from WAV cue markers

### FR-ASSET-05: WAD2 Graphics Loading
- Load WAD2 file (typically `gfx.wad`)
- Parse directory of lumps (pictures, textures)
- On-demand access: `W_GetLumpName(name)` returns raw data pointer
- Used for: console character font, status bar numbers, menu graphics

### FR-ASSET-06: Precache System
- Server maintains `sv.model_precache[256]` and `sv.sound_precache[256]`
- QuakeC calls `precache_model()` and `precache_sound()` during spawn
- After spawn, precache arrays are fixed (runtime additions cause error)
- Client receives precache lists and loads assets locally
- Model index 0 reserved; world BSP is always index 1

### FR-ASSET-07: Model Cache
- `Mod_ForName(name, crash)` first checks cache: `mod_known[MAX_MOD_KNOWN]`
- If cached AND data still in memory: return immediately
- If cached but data purged: reload from disk
- `MAX_MOD_KNOWN` = 256 models tracked simultaneously
- `Mod_ClearAll()` on level change marks models as potentially stale

## Implementation Files

| File | Purpose |
|------|---------|
| `model.c` | BSP, alias, sprite loader (software renderer) |
| `model.h` | Model data structures: `model_t`, `mvertex_t`, `msurface_t`, etc. |
| `gl_model.c` | Model loading (OpenGL variant) |
| `gl_model.h` | OpenGL model structures |
| `snd_mem.c` | WAV file loading and resampling |
| `wad.c` | WAD2 archive loading and lump access |
| `wad.h` | WAD2 types |
| `common.c` | File I/O: `COM_LoadFile`, `COM_FindFile` |
| `bspfile.h` | BSP on-disk format definitions |
| `modelgen.h` | MDL on-disk format definitions |
| `spritegn.h` | SPR on-disk format definitions |
| `sv_main.c` | Precache array management |

## Dependencies
- **Filesystem** (`common.c`): All file loading via PAK/search path
- **Memory** (`zone.c`): Hunk for permanent data, Cache for purgeable
- **Renderer** (`r_main.c`, `gl_rmain.c`): Consuming loaded models/textures
- **Sound** (`snd_dma.c`): Consuming loaded sound data
- **Server** (`sv_main.c`): Precache coordination

## Acceptance Criteria
- AC-01: BSP maps load correctly with all lumps parsed and validated
- AC-02: Alias models display correct geometry, skins, and animations
- AC-03: Sprites render with correct orientation and animation
- AC-04: Sounds play at correct pitch and volume after loading
- AC-05: WAD2 graphics display correctly (console font, menu items)
- AC-06: Precache system prevents loading assets after spawn completes
- AC-07: Model cache prevents redundant disk reads for shared assets
- AC-08: Level transitions properly free previous level's assets
- AC-09: Cache eviction doesn't crash when purgeable assets are reclaimed
