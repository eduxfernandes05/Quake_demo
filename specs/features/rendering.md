# Feature: 3D Rendering Engine

## Feature ID
`rendering`

## Purpose
Provide real-time 3D graphics rendering of the game world, including BSP environments, alias models (characters/objects), sprites, particles, and 2D HUD overlays. Supports two rendering paths: software rasterization and hardware-accelerated OpenGL.

## Scope
- BSP world rendering with PVS (Potentially Visible Set) culling
- Alias model rendering (MDL format) with animation and lighting
- Sprite rendering (SPR format) for effects
- Particle system for explosions, blood, etc.
- Dynamic lighting (up to 32 lights)
- Lightmap-based static lighting
- Liquid surface warping (water, lava, slime)
- Sky rendering (scrolling textures)
- View weapon model rendering
- Screen effects (palette shifts for damage, powerups, underwater)
- 2D drawing (console, menus, HUD)

## User Workflows

### Software Rendering Path
1. Engine initializes video mode via platform driver (`vid_*.c`)
2. Each frame, `R_RenderView()` is called from `V_RenderView()`
3. BSP tree is traversed front-to-back using `R_RenderWorld()`
4. PVS data culls invisible leaves; frustum culling removes off-screen surfaces
5. Surfaces are added to edge list; span-buffer resolves visibility
6. Surfaces are drawn with perspective-correct texture mapping
7. Alias models are drawn with affine texture mapping and Gouraud shading
8. Particles are rasterized as individual pixels
9. View weapon is drawn last (overlay)
10. 2D elements drawn on top (status bar, console)
11. Final framebuffer is blitted to screen via platform driver

### OpenGL Rendering Path
1. Engine initializes OpenGL context via platform driver (`gl_vid*.c`)
2. Each frame, `R_RenderView()` traverses BSP with `R_DrawWorld()`
3. Multi-pass rendering: base texture → lightmap → dynamic lights
4. Alias models rendered as indexed triangle meshes
5. Particles drawn as textured quads or points
6. Transparent surfaces (water) drawn with alpha blending
7. OpenGL handles rasterization, Z-buffering, and display

## Functional Requirements

### FR-REND-01: BSP World Rendering
- Traverse BSP tree to determine visible surfaces
- Use PVS compressed bitfield for leaf-to-leaf visibility
- Apply frustum culling to nodes and leaves
- Render surfaces with correct texture mapping and lightmaps
- Support animated textures (frame sequences)
- Support scrolling sky textures

### FR-REND-02: Alias Model Rendering  
- Load MDL format models with multiple skins and frames
- Interpolate between animation frames (GL path)
- Apply vertex lighting based on world lightlevel
- Support model scaling and rotation
- Render with backface culling

### FR-REND-03: Dynamic Lighting
- Support up to 32 simultaneous dynamic lights
- Update lightmaps in real-time for software path
- Additive light accumulation on surfaces
- Light radius attenuation (linear falloff)

### FR-REND-04: Particle System
- Fixed pool of particles (2048 max in `r_part.c`)
- Gravity-affected trajectories
- Color ramp animations (e.g., explosion yellow→red)
- Types: explosion, blood, rocket trail, teleport, etc.

### FR-REND-05: View Effects
- Palette shift blending for: damage (red), powerups (quad=purple, invulnerability=gold, envirosuit=green, ring of shadows=gray)
- Underwater warp effect
- View kick on damage
- Intermission camera (static view from info_intermission entity)

### FR-REND-06: 2D Drawing
- Character drawing from WAD-loaded font (`Draw_Character`)
- Picture loading and drawing (`Draw_Pic`, `Draw_TransPic`)
- Console background with translucency
- Crosshair rendering
- Tile-clear for letterboxing

## Implementation Files

### Software Renderer Core
| File | Purpose |
|------|---------|
| `r_main.c` | Render entry point, setup, `R_RenderView()` |
| `r_bsp.c` | BSP traversal, surface emission |
| `r_edge.c` | Edge list processing, span generation |
| `r_surf.c` | Surface caching and drawing |
| `r_draw.c` | Span drawing routines |
| `r_part.c` | Particle rendering |
| `r_light.c` | Lighting calculations, dynamic lightmaps |
| `r_misc.c` | Initialization, screen transforms |
| `r_alias.c` | Alias model (MDL) rendering |
| `r_sprite.c` | Sprite rendering |
| `r_sky.c` | Sky texture rendering |
| `r_aclip.c` | Alias model clipping |
| `r_efrag.c` | Entity fragment storage (leaf distribution) |
| `r_shared.h` | Shared renderer types |
| `r_local.h` | Software renderer internal types |

### Software Renderer (x86 Assembly)
| File | Purpose |
|------|---------|
| `r_aclipa.s` | Alias clipping (assembly) |
| `r_aliasa.s` | Alias model drawing (assembly) |
| `r_drawa.s` | Span drawing (assembly) |
| `r_edgea.s` | Edge processing (assembly) |
| `d_draw.s` | Low-level drawing (assembly) |
| `d_draw16.s` | 16-pixel span drawing |
| `d_parta.s` | Particle drawing (assembly) |
| `d_scana.s` | Scanline processing |
| `d_spr8.s` | Sprite drawing |
| `d_copy.s` | Framebuffer copy |
| `surf8.s` / `surf16.s` | Surface drawing |

### OpenGL Renderer
| File | Purpose |
|------|---------|
| `gl_draw.c` | 2D drawing (OpenGL) |
| `gl_mesh.c` | Alias model mesh processing |
| `gl_model.c` | Model loading (OpenGL variant) |
| `gl_refrag.c` | Entity fragment management |
| `gl_rlight.c` | Dynamic lighting (OpenGL) |
| `gl_rmain.c` | Render entry point (OpenGL) |
| `gl_rmisc.c` | Miscellaneous render setup |
| `gl_rsurf.c` | Surface rendering (OpenGL) |
| `gl_screen.c` | Screen management (OpenGL) |
| `gl_warp.c` | Warp surface rendering |
| `gl_vidnt.c` | Windows OpenGL video driver |
| `gl_vidlinuxglx.c` | Linux GLX video driver |
| `glquake.h` | OpenGL renderer header |

### Video Drivers
| File | Purpose |
|------|---------|
| `vid_win.c` | Windows video (DIB/MGL) |
| `vid_svgalib.c` | Linux SVGALib |
| `vid_x.c` | Linux X11/XShm |
| `vid_dos.c` / `vid_dos.h` | DOS video |
| `vid_ext.c` | Extended video modes |

### Shared
| File | Purpose |
|------|---------|
| `screen.c` | Screen management, SCR_UpdateScreen |
| `view.c` | View setup, palette shifts, V_RenderView |
| `draw.c` | 2D drawing primitives |
| `d_edge.c` | Edge scan conversion |
| `d_fill.c` | Solid fill |
| `d_init.c` | Drawing initialization |
| `d_modech.c` | Mode change handling |
| `d_scan.c` | Texture-mapped scan conversion |
| `d_sprite.c` | Sprite scan conversion |
| `d_vars.c` | Global drawing variables |
| `d_zpoint.c` | Z-buffered point drawing |
| `render.h` | Renderer public interface |
| `vid.h` | Video subsystem interface |

## Configuration (CVars)
| CVar | Default | Description |
|------|---------|-------------|
| `r_drawentities` | 1 | Draw entities |
| `r_drawviewmodel` | 1 | Draw view weapon |
| `r_speeds` | 0 | Show render statistics |
| `r_fullbright` | 0 | Disable lighting |
| `r_lightmap` | 0 | Show lightmaps only |
| `r_shadows` | 0 | Draw entity shadows |
| `r_wateralpha` | 1 | Water transparency (GL) |
| `r_dynamic` | 1 | Enable dynamic lighting |
| `gl_cull` | 1 | Backface culling (GL) |
| `gl_smoothmodels` | 1 | Smooth model shading (GL) |
| `gl_affinemodels` | 0 | Affine texture mapping (GL) |
| `gl_flashblend` | 1 | Blend dynamic lights (GL) |
| `gl_nocolors` | 0 | Disable player colors (GL) |

## Dependencies
- **Model Loader** (`model.c` / `gl_model.c`): BSP, MDL, SPR loading
- **Zone Memory** (`zone.c`): Surface cache allocation
- **Common** (`common.c`): File system for asset loading
- **Sound** (`snd_dma.c`): No direct dependency but synchronized via frame

## Acceptance Criteria
- AC-01: BSP world renders correctly with no Z-fighting or missing surfaces
- AC-02: PVS culling eliminates non-visible geometry
- AC-03: Alias models animate smoothly at frame rate
- AC-04: Dynamic lights illuminate nearby surfaces in real-time
- AC-05: Particles spawn, move under gravity, and fade correctly
- AC-06: View effects (damage red flash, underwater warp) display on cue
- AC-07: Software and GL paths produce visually consistent results
- AC-08: Minimum 30 FPS at 320×200 on target hardware (Pentium 75MHz)
