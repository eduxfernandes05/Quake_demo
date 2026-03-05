# Feature: HUD and Status Bar

## Feature ID
`hud-status-bar`

## Purpose
Display real-time player status information overlaid on the game view, including health, armor, ammunition, weapons, items, and score. The HUD provides essential gameplay feedback through graphical indicators.

## Scope
- Status bar with health, armor, ammo, face indicator
- Weapon inventory bar (top of status bar)
- Intermission scoreboard (between levels)
- Deathmatch scoreboard (Tab key overlay)
- Mini deathmatch overlay
- Support for mission pack variants (Hipnotic, Rogue)

## User Workflows

### Normal Gameplay
1. Status bar displays at bottom of screen (always visible)
2. Health shown as numeric value with player face icon (expression changes with health)
3. Current ammo count for active weapon
4. Armor type (green/yellow/red) and value
5. Weapon icons highlight owned and active weapons
6. Item icons show active powerups (quad damage, invulnerability, etc.)
7. Sigil (rune) icons show collected episode keys

### Scoreboard
1. During gameplay: press Tab to toggle scoreboard overlay
2. Shows all players: name, frags, connection time
3. Deathmatch: sorted by frag count
4. Cooperative: shows kills alongside frags

### Level Intermission
1. Level ends → intermission screen with stats
2. Shows: kill count, secret count, time
3. Multiplayer: full scoreboard with rankings
4. Press any key to proceed to next level

### Screen Size Adjustment
1. Player adjusts `viewsize` slider in options or `+`/`-` keys
2. Status bar scales: full → no weapon bar → mini → invisible
3. At maximum viewsize, HUD elements overlay on game view

## Functional Requirements

### FR-HUD-01: Status Bar Components
| Element | Position | Information |
|---------|----------|------------|
| Face icon | Center | Health-dependent expression (6 states + gibbed + dead) |
| Health | Center-right | Numeric value (3 digits) |
| Armor icon | Left | Green/yellow/red armor type |
| Armor value | Left | Numeric value (3 digits) |
| Ammo icon | Right-left | Type icon (shells/nails/rockets/cells) |
| Ammo count | Right | Numeric value (3 digits) |
| Weapon bar | Top | 7 weapon slot icons (highlighted when owned, flash on pickup) |
| Ammo counts (bar) | Below weapons | Count for each ammo type |
| Items | Center-bottom | Active powerup icons |
| Sigils | Right-bottom | Episode key (rune) icons |

### FR-HUD-02: Face Icon States
- Gibbed: health ≤ -40 (pile of gore)
- Dead: health ≤ 0 (dead face)
- 5 alive states based on health: 80-100, 60-79, 40-59, 20-39, 0-19
- Pain animation: flinch on damage
- Active powerup overlays: invisible, quad, invulnerability, invisible+invulnerable

### FR-HUD-03: Screen Size Modes
| viewsize | Status Bar | Effect |
|----------|-----------|--------|
| 30-99 | Full | Status bar + weapon bar at bottom |
| 100-109 | Minimal | Status bar only, no weapon bar |
| 110-119 | Overlay | Numbers over game view, no bar background |
| 120 | None | Completely hidden |

### FR-HUD-04: Deathmatch Scoreboard
- Triggered by Tab key (`Sbar_ShowScores`/`Sbar_DontShowScores`)
- Displays all players sorted by frags
- Shows: rank, name, frags, shirt/pants colors, ping
- Mini overlay: persistent smaller scoreboard in corner (4 players)

### FR-HUD-05: Intermission Screen
- Full-screen overlay replacing gameplay
- Single-player stats: kills `x/y`, secrets `x/y`, time `mm:ss`
- Multiplayer: full deathmatch scoreboard
- Player presses key or waits for server to advance

### FR-HUD-06: Mission Pack Support
- **Hipnotic** (Scourge of Armagon): Additional weapon icons (laser cannon, mjolnir, proximity gun), status items
- **Rogue** (Dissolution of Eternity): Additional weapon icons, team color border, alternate ammo types

### FR-HUD-07: Number Drawing
- Custom font: `sb_nums[2][11]` — two color sets (standard/highlighted), digits 0-9 plus minus sign
- Colon and slash for ratios
- Drawn at fixed positions on status bar graphic

## Implementation Files

| File | Purpose |
|------|---------|
| `sbar.c` | All HUD rendering: status bar, scoreboard, intermission (~700 lines) |
| `sbar.h` | HUD interface |
| `screen.c` | Screen size management, `SCR_UpdateScreen` integration |
| `gl_screen.c` | OpenGL screen management |
| `draw.c` | 2D drawing primitives for HUD elements |
| `cl_parse.c` | Receives stat updates from server (`svc_updatestat`) |
| `client.h` | `cl.stats[]`: health, frags, weapon, ammo, armor, items |

## Configuration (CVars)
| CVar | Default | Description |
|------|---------|-------------|
| `scr_viewsize` | 100 | Screen size / HUD visibility (30-120) |
| `crosshair` | 0 | Show crosshair |
| `scr_showturtle` | 0 | Show turtle icon on slow frames |
| `showfps` | 0 | FPS counter (if implemented) |

## Dependencies
- **Client** (`cl_parse.c`): Stat updates from server set `cl.stats[]`
- **Draw** (`draw.c`): All 2D rendering primitives
- **Screen** (`screen.c`): Integration with screen refresh cycle
- **View** (`view.c`): Intermission detection affects rendering

## Acceptance Criteria
- AC-01: Health, armor, and ammo update in real-time as gameplay values change
- AC-02: Face icon changes expression based on health and powerup state
- AC-03: Weapon bar correctly highlights owned and active weapons
- AC-04: Tab scoreboard shows all players with correct frag counts
- AC-05: Intermission displays accurate level completion stats
- AC-06: Screen size adjustment smoothly transitions HUD visibility
- AC-07: HUD renders correctly at all supported resolutions
- AC-08: Mission pack (Hipnotic/Rogue) elements display when active
