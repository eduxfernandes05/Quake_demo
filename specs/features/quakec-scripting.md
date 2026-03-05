# Feature: QuakeC Scripting VM

## Feature ID
`quakec-scripting`

## Purpose
Execute compiled QuakeC bytecode (`progs.dat`) to implement all game logic — weapon behavior, monster AI, item pickups, triggers, level scripting, and game rules. The QuakeC VM is a stack-based interpreter embedded in the server that operates on game entities (edicts).

## Scope
- Bytecode interpreter for compiled QuakeC programs
- Entity (edict) management with QuakeC-accessible fields
- ~80 built-in functions callable from QuakeC (C-implemented)
- String table, global variables, and per-entity field data
- Integration with server physics, networking, and entity spawning
- Error handling and runtime debugging

## User Workflows

### Game Logic Execution
1. Server loads `progs.dat` from game directory during `SV_SpawnServer`
2. CRC of progs is verified against expected value for compatibility
3. Global definitions, field definitions, functions, and statements are loaded
4. For each entity in BSP, spawn function is called (e.g., `monster_ogre`, `item_health`)
5. Each server frame: entity `think` functions execute at scheduled times
6. Player input triggers: `PlayerPreThink` → physics → `PlayerPostThink`
7. Collision events trigger `touch` function pairs
8. Combat triggers `th_pain` and `th_die` callbacks

### Modding Workflow
1. Modder writes QuakeC source files (`.qc`)
2. Compiles with QCCC or fteqcc to produce `progs.dat`
3. Places `progs.dat` in mod directory (e.g., `mymod/progs.dat`)
4. Players run with `-game mymod` command-line argument
5. Engine loads modified game logic; all standard features can be changed

## Functional Requirements

### FR-QC-01: Bytecode Interpreter
- 62 opcodes covering: arithmetic (float, vector), comparison, branching, load/store, function call/return, indirect access
- Stack-based execution with local variable stack
- Maximum call depth: 32 (`MAX_STACK_DEPTH`)
- Local stack size: 2048 entries (`LOCALSTACK_SIZE`)
- Globals accessed by offset; entity fields by offset + edict base

### FR-QC-02: Type System
- `void`, `float`, `string`, `vector` (3 floats), `entity`, `field`, `function`, `pointer`
- All values are 32-bit (float or int, context-dependent)
- Vectors stored as 3 consecutive floats
- Strings are indices into a global string table
- Entities are edict indices

### FR-QC-03: Entity Management
- `ED_Alloc()`: Allocate new edict from pool (max 600)
- `ED_Free()`: Mark edict as free (reusable after 0.5s delay)
- `ED_ParseEdict()`: Parse key-value pairs into edict fields
- `ED_Write()`: Serialize edict to text for save files
- Entity fields defined by `fielddefs` in progs.dat header

### FR-QC-04: Built-in Functions (~80)
Core categories:
- **Math**: `sin`, `cos`, `sqrt`, `rint`, `floor`, `ceil`, `fabs`, `random`, `normalize`, `vlen`, `vectoangles`, `vectoyaw`
- **Entity**: `spawn`, `remove`, `find`, `findradius`, `nextent`, `setmodel`, `setsize`, `setorigin`
- **Physics**: `traceline`, `droptofloor`, `walkmove`, `movetogoal`, `checkbottom`, `pointcontents`
- **Sound**: `sound`, `ambientsound`
- **String**: `ftos`, `vtos`, `sprint`, `bprint`, `dprint`, `centerprint`, `stuffcmd`
- **Combat**: `aim`, `particle`, `lightstyle`
- **Server**: `makestatic`, `precache_sound`, `precache_model`, `precache_file`, `setspawnparms`, `changelevel`
- **File**: `WriteAngle`, `WriteCoord`, `WriteByte`, `WriteChar`, `WriteShort`, `WriteLong`, `WriteString`, `WriteEntity` (message writing to clients)
- **AI**: `ai_forward`, `changeyaw`, `checklos` (wrapped by QuakeC)

### FR-QC-05: Event Callbacks
| Event | QuakeC Function | Trigger |
|-------|----------------|---------|
| Player frame (pre) | `PlayerPreThink` | Every server frame per player |
| Player frame (post) | `PlayerPostThink` | After physics per player |
| Entity think | `self.think` | When `self.nextthink <= time` |
| Touch | `self.touch` | Two entities overlap |
| Use | `self.use` | Entity activated |
| Blocked | `self.blocked` | MOVETYPE_PUSH entity blocked |
| Client connect | `ClientConnect` | Player joins server |
| Client disconnect | `ClientDisconnect` | Player leaves |
| Level entry | `PutClientInServer` | Player spawns into level |
| Client kill | `ClientKill` | Player types `kill` |
| Level change | `SetChangeParms` | Before level transition |
| New game | `SetNewParms` | First level of new game |

### FR-QC-06: Debugging Support
- `pr_trace` flag enables instruction tracing to console
- `edict <num>` console command dumps entity fields
- `edicts` prints all active entity count
- `edictcount` shows allocation stats
- Runtime errors print function name, file, and statement number
- `PR_RunError()` terminates current execution with diagnostic

## Implementation Files

| File | Purpose |
|------|---------|
| `pr_exec.c` | Bytecode interpreter (`PR_ExecuteProgram`), stack management |
| `pr_edict.c` | Entity management, field parsing, spawn, free, write |
| `pr_cmds.c` | All built-in functions (C implementations of ~80 functions) |
| `pr_comp.h` | Bytecode format: opcodes, types, statement structure |
| `progs.h` | Runtime types: `edict_t`, globals, `PR_ExecuteProgram` interface |
| `progdefs.h` | `globalvars_t`, `entvars_t` — C struct mapping of QuakeC globals/fields |
| `sv_main.c` | Server integration: calls QuakeC callbacks |
| `sv_phys.c` | Physics: triggers touch/think callbacks |
| `sv_user.c` | Player processing: calls PreThink/PostThink |

## Configuration
| CVar | Default | Description |
|------|---------|-------------|
| `pr_checkextension` | 0 | (Future) Extension function checking |
| `nomonsters` | 0 | Skip spawning monsters (command line) |
| `developer` | 0 | Enable debug-level QuakeC output |

## Dependencies
- **Server** (`sv_main.c`, `sv_phys.c`): Scheduling and callback invocation
- **Entity System** (`server.h`): Edict pool allocation and management
- **Network** (protocol): Built-in message writing functions
- **Filesystem** (`common.c`): Loading `progs.dat` from game directory
- **Zone Memory** (`zone.c`): Hunk allocation for progs data

## Acceptance Criteria
- AC-01: Standard `progs.dat` (id1) executes all game logic correctly
- AC-02: Monsters follow AI paths, attack player, react to damage
- AC-03: Items can be picked up with correct QuakeC-defined behavior
- AC-04: Triggers fire correctly (doors, buttons, teleporters, messages)
- AC-05: Level transitions preserve state via `SetChangeParms`
- AC-06: Custom mods load and run via `-game` parameter
- AC-07: Runtime errors produce diagnostic output without crashing engine
- AC-08: Entity limit of 600 is enforced; graceful handling when exhausted
