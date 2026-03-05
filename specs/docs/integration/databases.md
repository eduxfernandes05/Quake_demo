# Database Schemas and Data Models

## Overview

WinQuake uses **no relational database**. All persistent data is stored as binary archive files (PAK), compiled bytecode (progs.dat), and text configuration files. In-memory data structures are defined in C headers and loaded from disk at startup or level change.

## File Formats

### PAK Archive Format (`common.c`)

PAK files are the primary asset container:

```
┌──────────────────────────────┐
│ Header (12 bytes)            │
│   char[4] id = "PACK"       │
│   int     dirofs             │
│   int     dirlen             │
├──────────────────────────────┤
│ File Data (variable)         │
│   Raw file contents...       │
├──────────────────────────────┤
│ Directory (at dirofs)        │
│   For each file:             │
│     char[56] name            │
│     int      filepos         │
│     int      filelen         │
└──────────────────────────────┘
```

### BSP Map Format (`bspfile.h`)

Binary Space Partitioning map file, version 29:

```
Header:
  int version = 29 (BSPVERSION)
  lump_t lumps[15]    // offset + length for each data section

Lumps:
  0  ENTITIES    - text entity definitions
  1  PLANES      - splitting planes
  2  TEXTURES    - texture data (mipmap chains)
  3  VERTEXES    - vertex positions
  4  VISIBILITY  - PVS compressed bitfield
  5  NODES       - BSP tree nodes
  6  TEXINFO     - texture mapping info
  7  FACES       - polygon faces
  8  LIGHTING    - lightmap data
  9  CLIPNODES   - collision hulls
  10 LEAFS       - BSP tree leaves (containing faces)
  11 MARKSURFACES - leaf-to-face mapping
  12 EDGES       - edge pairs (vertex indices)
  13 SURFEDGES   - ordered edge references per face
  14 MODELS      - brush model data (submodels)
```

**Design Limits** (from `bspfile.h`):
| Limit | Value |
|-------|-------|
| `MAX_MAP_HULLS` | 4 |
| `MAX_MAP_MODELS` | 256 |
| `MAX_MAP_PLANES` | 32767 |
| `MAX_MAP_NODES` | 32767 |
| `MAX_MAP_CLIPNODES` | 32767 |
| `MAX_MAP_LEAFS` | 8192 |
| `MAX_MAP_VERTS` | 65535 |
| `MAX_MAP_FACES` | 65535 |
| `MAX_MAP_EDGES` | 256000 |
| `MAX_MAP_TEXTURES` | 512 |
| `MAX_MAP_LIGHTING` | 0x100000 (1MB) |
| `MAX_MAP_VISIBILITY` | 0x100000 (1MB) |

### Alias Model Format (MDL) — `modelgen.h`

Character and object models:

```
Header:
  int     ident = "IDPO"     // magic number
  int     version = 6
  vec3_t  scale              // model scale
  vec3_t  scale_origin       // scale offset
  float   boundingradius
  vec3_t  eyeposition
  int     numskins
  int     skinwidth, skinheight
  int     numverts
  int     numtris
  int     numframes
  int     synctype            // animation sync
  int     flags

Data:
  Skin data (indexed color, variable number of skins/groups)
  Texture coordinates (s, t per vertex)
  Triangles (3 vertex indices + facing flag)
  Frames (vertex positions as compressed bytes + normals)
```

### Sprite Format (SPR) — `spritegn.h`

2D billboard sprites:

```
Header:
  int     ident = "IDSP"
  int     version = 1
  int     type               // orientation type
  float   boundingradius
  int     width, height
  int     numframes

Data:
  Frame groups (origin offset + pixel data per frame)
```

### WAD2 Format — `wad.h`

Graphics archive (UI elements, console font):

```
Header:
  char[4] identification = "WAD2"
  int     numlumps
  int     infotableofs

Directory entries:
  int     filepos
  int     disksize
  int     size
  char    type               // 'B' = picture, 'D' = miptex, 'E' = palette
  char    compression
  char[16] name
```

### QuakeC Progs Format — `pr_comp.h`

Compiled QuakeC bytecode (`progs.dat`):

```
Header (dprograms_t):
  int     version = 6
  int     crc                // source CRC for validation
  int     ofs_statements     // bytecode instructions
  int     numstatements
  int     ofs_globaldefs     // global variable definitions
  int     numglobaldefs
  int     ofs_fielddefs      // entity field definitions
  int     numfielddefs
  int     ofs_functions      // function definitions
  int     numfunctions
  int     ofs_strings        // string table
  int     numstrings
  int     ofs_globals        // global variable data
  int     numglobals
  int     entityfields       // size of entity data
```

**QuakeC Type System** (`pr_comp.h`):
- `ev_void`, `ev_string`, `ev_float`, `ev_vector`, `ev_entity`, `ev_field`, `ev_function`, `ev_pointer`
- Instructions: 62 opcodes (arithmetic, comparison, load/store, flow control)

### Save Game Format — `host_cmd.c`

Text file:
```
<version>              // save version number
<comment>              // description (map name, kills)
<spawn_parms>          // 16 player parameters
<skill>                // difficulty level
<map_name>             // current map
<server_time>          // game time
<light_styles>         // 64 light animation strings
{                      // entity definitions (key-value pairs)
"classname" "player"
"origin" "480 -352 88"
"health" "100"
...
}
```

## In-Memory Data Structures

### Entity System

**`edict_t`** (in `progs.h`) — Game entity:
```c
typedef struct edict_s {
    qboolean    free;               // available for reuse
    link_t      area;               // spatial partitioning link
    int         num_leafs;          // BSP leaves this entity touches
    short       leafnums[16];       // leaf indices
    entity_state_t baseline;        // network baseline
    float       freetime;           // when freed (anti-reuse delay)
    entvars_t   v;                  // QuakeC-accessible fields
    // additional QuakeC fields follow...
} edict_t;
```

**`entvars_t`** (in `progdefs.h`) — QuakeC fields accessible from C:
- Position: `origin`, `angles`, `velocity`, `avelocity`
- Model: `modelindex`, `model`, `frame`, `skin`, `effects`
- Physics: `movetype`, `solid`, `mins`, `maxs`, `size`, `gravity`
- Combat: `health`, `takedamage`, `dmg_take`, `dmg_save`, `dmg_inflictor`
- AI: `enemy`, `goalentity`, `ideal_yaw`, `yaw_speed`
- Callbacks: `think`, `touch`, `use`, `blocked`, `th_die`, `th_pain`
- Player: `weapon`, `weaponmodel`, `currentammo`, `items`, `armorvalue`

### Client State

**`client_state_t`** (`client.h`) — Per-connection client data:
- `stats[32]` — health, frags, weapon, ammo, armor
- `items` — inventory bitmask
- `viewangles`, `velocity`, `punchangle`
- `mtime[2]` — server timestamps for interpolation
- Entity arrays of up to 256 models, 256 sounds
- Dynamic light, beam, effect lists

**`client_static_t`** (`client.h`) — Persistent client data:
- `state` — `ca_dedicated`, `ca_disconnected`, `ca_connected`
- Demo recording/playback state
- Network connection handle
- Message buffer for outgoing data

### Server State

**`server_t`** (`server.h`) — Active server:
- `name[64]` — map name
- `worldmodel` — BSP world
- `model_precache[256]`, `sound_precache[256]`
- `edicts` — entity array (max 600)
- Datagram buffers for reliable and unreliable data

**`server_static_t`** (`server.h`) — Persistent server:
- `maxclients`, `clients[]` — connected clients
- `serverflags` — episode completion tracking

### Limits Summary
| Resource | Maximum | Defined In |
|----------|---------|-----------|
| Entities (edicts) | 600 | `quakedef.h` |
| Models | 256 | `quakedef.h` |
| Sounds | 256 | `quakedef.h` |
| Light styles | 64 | `quakedef.h` |
| Players | 16 | `quakedef.h` |
| Dynamic lights | 32 | `client.h` |
| Beams | 24 | `client.h` |
| Entity fragments | 640 | `client.h` |
| Sound channels | 8 | `quakedef.h` |
| Scoreboard entries | 16 | `quakedef.h` |
