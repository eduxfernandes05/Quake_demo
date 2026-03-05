# Feature: Physics and World Simulation

## Feature ID
`physics-world`

## Purpose
Simulate entity movement, collision detection, and physical interactions within the BSP world. The physics system runs server-side and handles all entity types: players, monsters, projectiles, doors, platforms, and environmental objects.

## Scope
- Multiple movement types (walk, fly, swim, toss, push, bounce, noclip)
- BSP-based collision detection using clip hulls
- Trace-line and trace-box operations for line-of-sight and movement
- Gravity, friction, and acceleration
- Trigger volumes and touch callbacks
- Area links for efficient entity-entity collision testing

## User Workflows

### Player Movement
1. Client sends `clc_move` with desired velocity and view angles
2. Server processes in `SV_ReadClientMove` → `SV_ClientThink`
3. `SV_RunClients` applies physics:
   - Ground detection (on floor, in air, in water)
   - Friction (ground friction, water friction)
   - Acceleration (air, ground, water)
   - Gravity application
   - Clip movement against world and entities
4. New position sent to clients via entity update

### Projectile Physics
1. QuakeC sets entity `movetype = MOVETYPE_FLYMISSILE` or `MOVETYPE_TOSS`
2. Each server frame, `SV_Physics_Toss` / `SV_Physics_Step` runs
3. Velocity integrated: `origin += velocity * frametime`
4. Gravity applied (for TOSS): `velocity.z -= sv_gravity * frametime`
5. Collision tested via `SV_FlyMove`
6. On impact: `touch` function called on both entities

### Door/Platform Movement
1. QuakeC creates door/platform as `MOVETYPE_PUSH`, `SOLID_BSP`
2. Entity has target position and movement speed
3. `SV_Physics_Pusher` moves entity and pushes blocking entities
4. If push is blocked, entity may reverse or stop (per QuakeC logic)
5. Touch triggers fire when player contacts push entity

### Environmental Physics
1. Items: `MOVETYPE_TOSS` — fall under gravity, rest on ground
2. Corpses/gibs: `MOVETYPE_TOSS` — scatter and settle
3. Triggers: `SOLID_TRIGGER` — detect overlap without collision
4. Static entities: `MOVETYPE_NONE` — no physics processing

## Functional Requirements

### FR-PHYS-01: Movement Types
| Type | Value | Behavior |
|------|-------|----------|
| `MOVETYPE_NONE` | 0 | No movement, no gravity |
| `MOVETYPE_ANGLENOCLIP` | 1 | Only angular movement, no clipping |
| `MOVETYPE_ANGLECLIP` | 2 | Angular movement with clipping |
| `MOVETYPE_WALK` | 3 | Player walking (gravity, stairs, friction) |
| `MOVETYPE_STEP` | 4 | Monster stepping (gravity, stair-step) |
| `MOVETYPE_FLY` | 5 | Free movement, no gravity |
| `MOVETYPE_TOSS` | 6 | Gravity-affected projectile (stops on ground) |
| `MOVETYPE_PUSH` | 7 | BSP mover (doors, plats); pushes blockers |
| `MOVETYPE_NOCLIP` | 8 | No clipping, free movement (debug) |
| `MOVETYPE_FLYMISSILE` | 9 | Like FLY, triggers touch on any contact |
| `MOVETYPE_BOUNCE` | 10 | Like TOSS but bounces off surfaces |

### FR-PHYS-02: Solid Types
| Type | Value | Behavior |
|------|-------|----------|
| `SOLID_NOT` | 0 | No collision (corpses, effects) |
| `SOLID_TRIGGER` | 1 | Overlap detection only (triggers) |
| `SOLID_BBOX` | 2 | Axis-aligned bounding box collision |
| `SOLID_SLIDEBOX` | 3 | BBOX + sliding (players, monsters) |
| `SOLID_BSP` | 4 | BSP model collision (doors, world) |

### FR-PHYS-03: Collision Detection
- **World clipping**: BSP clip hulls (3 sizes: point, player, large monster)
- `SV_Move(start, mins, maxs, end, type, entity)`: Trace movement through world + entities
- Trace types: `MOVE_NORMAL`, `MOVE_NOMONSTERS`, `MOVE_MISSILE`
- Returns: fraction complete, end position, surface normal, hit entity
- Stair stepping: auto-step up 18 units for WALK and STEP movetypes

### FR-PHYS-04: Gravity and Friction
- Gravity: `sv_gravity` (default 800 units/s²), applied per frame: `vel.z -= gravity * frametime`
- Ground friction: `sv_friction` (default 4), deceleration when on ground
- Edge friction: `sv_edgefriction` (default 2), extra friction near ledges
- Stop speed: `sv_stopspeed` (default 100), minimum speed threshold
- Max velocity: `sv_maxvelocity` (default 2000), velocity clamping per axis

### FR-PHYS-05: Area Links
- Entities linked into BSP leaves via `SV_LinkEdict`
- `areanode_t` tree divides world into regions
- Entity-entity collision checks limited to overlapping regions
- `SV_AreaEdicts` returns entities in a bounding box
- Efficient spatial query for `findradius`, `traceline`, etc.

### FR-PHYS-06: Water Physics
- Water levels: 0 (dry), 1 (feet), 2 (waist), 3 (eyes submerged)
- Reduced gravity in water
- Water friction replaces ground friction
- Drowning damage when submerged without air supply
- Point contents check: `SV_PointContents` → CONTENTS_WATER, CONTENTS_SLIME, CONTENTS_LAVA

### FR-PHYS-07: Push Physics
- `SV_Physics_Pusher`: Moves BSP entities (doors, elevators)
- All entities in the path are pushed along
- If blocked and no clearance: `blocked` callback fires
- Angular rotation supported (rotating doors, etc.)
- Push entities can crush (damage on blocked with no escape)

## Implementation Files

| File | Purpose |
|------|---------|
| `sv_phys.c` | Physics simulation: all movement types, velocity integration, `SV_RunEntity` |
| `sv_move.c` | AI movement: `SV_movestep`, `SV_StepDirection`, `SV_NewChaseDir` |
| `sv_user.c` | Player physics: `SV_ClientThink`, `SV_ReadClientMove` |
| `world.c` | Collision: `SV_Move`, `SV_LinkEdict`, area links, hull tracing |
| `world.h` | World/collision interface |
| `server.h` | `movetype_t`, `solid_t` enums, server state |
| `mathlib.c` | Vector math: dot product, cross product, normalize, etc. |
| `mathlib.h` | Math types: `vec3_t`, plane operations |

### Assembly Optimizations
| File | Purpose |
|------|---------|
| `math.s` | x86 assembly math functions |
| `worlda.s` | Assembly hull tracing (if present) |

## Configuration (CVars)
| CVar | Default | Description |
|------|---------|-------------|
| `sv_gravity` | 800 | World gravity (units/s²) |
| `sv_friction` | 4 | Ground friction multiplier |
| `sv_edgefriction` | 2 | Edge friction multiplier |
| `sv_stopspeed` | 100 | Minimum speed before stopping |
| `sv_maxspeed` | 320 | Maximum player speed |
| `sv_maxvelocity` | 2000 | Maximum entity velocity per axis |
| `sv_accelerate` | 10 | Player acceleration |
| `sv_nostep` | 0 | Disable stair stepping |
| `sv_idealpitchscale` | 0.8 | Auto-pitch on slopes |

## Dependencies
- **BSP Model** (`model.c`): Hull data for collision testing
- **QuakeC VM** (`pr_exec.c`): `think`, `touch`, `blocked` callbacks
- **Network** (`sv_main.c`): Entity state sent to clients after physics
- **Math** (`mathlib.c`): Vector operations, angle computation

## Acceptance Criteria
- AC-01: Player walks on surfaces, falls under gravity, stops on ground
- AC-02: Stair stepping handles up to 18-unit steps smoothly
- AC-03: Projectiles follow correct trajectories (fly, toss, bounce)
- AC-04: Doors/platforms push entities without clipping through walls
- AC-05: Triggers fire touch callbacks on entity overlap
- AC-06: Water physics applies reduced gravity and friction
- AC-07: Collision detection prevents entities from passing through walls
- AC-08: `traceline` accurately detects line-of-sight through BSP
- AC-09: Entity-entity collisions resolve based on solid types
