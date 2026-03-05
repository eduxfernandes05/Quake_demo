# Task: Architecture Refactor — Headless Video Driver

**Phase**: 1 (Headless Game Worker)  
**Priority**: P0  
**Estimated Effort**: 5–8 days  
**Prerequisites**: CMake build system, NOASM build, sprintf fixes

## Objective

Create a headless video driver (`vid_headless.c`) that renders using Mesa LLVMpipe off-screen OpenGL and exports the framebuffer for external consumption, eliminating all display server dependencies.

## Acceptance Criteria

- [ ] Engine starts and renders without X11, Wayland, or any display server
- [ ] `vid_headless.c` initializes Mesa EGL + LLVMpipe off-screen context
- [ ] Frames render to FBO, readable via `VID_CaptureFrame()`
- [ ] `VID_CaptureFrame()` returns valid RGBA data at requested resolution
- [ ] First captured frame written to PNG matches expected scene (SSIM ≥ 0.85 vs reference)
- [ ] No display-related libraries linked (no libX11, no libwayland)
- [ ] Runs in Docker container without `--privileged` or display forwarding
- [ ] Sustained 30+ fps for e1m1 on 2 vCPU container

## Implementation Steps

### 1. Create vid_headless.c

Implement the video driver interface (`viddef_t`) for headless rendering:

```c
// vid_headless.c — Headless OpenGL video driver using Mesa EGL + LLVMpipe

#include <EGL/egl.h>
#include <GL/gl.h>
#include "quakedef.h"

static EGLDisplay egl_display;
static EGLContext egl_context;
static EGLSurface egl_surface;
static GLuint capture_fbo, capture_texture;
static uint8_t *capture_buffer;
static int capture_width = 1280;
static int capture_height = 720;

void VID_Init(unsigned char *palette)
{
    // Initialize EGL with GBM platform (headless)
    // Create off-screen pbuffer surface
    // Create OpenGL context
    // Create FBO + texture for capture
    // Allocate capture_buffer
}

void VID_Shutdown(void)
{
    // Destroy FBO, context, surface, display
    // Free capture_buffer
}

void VID_Update(vrect_t *rects)
{
    // glReadPixels from FBO into capture_buffer
    // (No display blit — headless)
}

qboolean VID_CaptureFrame(uint8_t **rgba, int *width, int *height)
{
    // Return pointer to capture_buffer
    *rgba = capture_buffer;
    *width = capture_width;
    *height = capture_height;
    return true;
}
```

### 2. Integrate with GL Rendering Path

- Use the GLQuake rendering path (`gl_*.c` files)
- Redirect GL context creation to EGL/LLVMpipe instead of GLX/WGL
- Ensure all GL calls target the FBO
- `GL_BeginRendering()` binds FBO
- `GL_EndRendering()` — no swap buffers, just glFinish

### 3. CMake Integration

```cmake
if(HEADLESS)
    add_definitions(-DHEADLESS)
    set(VID_SOURCES vid_headless.c)
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EGL REQUIRED egl)
    pkg_check_modules(GBM REQUIRED gbm)
    target_link_libraries(quake-worker OpenGL::GL ${EGL_LIBRARIES} ${GBM_LIBRARIES})
else()
    # Original vid_*.c files
endif()
```

### 4. Docker Validation

```bash
docker build -t quake-worker .
docker run --rm \
    -v /path/to/id1:/game/id1 \
    -e QUAKE_BASEDIR=/game \
    quake-worker +map e1m1 +captureframe test.rgba +quit
```

### 5. Performance Benchmark

```bash
# Run for 60s, measure frame times
docker run --rm --cpus=2 --memory=512m \
    -v /path/to/id1:/game/id1 \
    -e QUAKE_BASEDIR=/game \
    quake-worker +map e1m1 +timedemo demo1
```

Target: ≥ 30 fps average on 2 vCPU.

## Files to Create

- `WinQuake/vid_headless.c` — new headless video driver

## Files to Modify

- `CMakeLists.txt` — add headless video driver option
- `Dockerfile` — add Mesa EGL/GBM dependencies
- `gl_vidlinuxglx.c` (or equivalent) — extract common GL init code if reusable

## Dependencies

- Mesa (libGL, libEGL, libGBM, LLVMpipe driver)
- EGL headers and libraries

## Risks

- LLVMpipe may not support all GL extensions used by GLQuake — test early
- Frame capture via glReadPixels may be slow — benchmark; consider PBO async readback
- If GL path too complex, fallback: use software renderer and read from `vid.buffer`

## Rollback

- Remove `vid_headless.c`, revert CMakeLists.txt
- Engine falls back to original video drivers
