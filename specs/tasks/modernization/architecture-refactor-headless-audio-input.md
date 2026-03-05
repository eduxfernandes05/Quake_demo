# Task: Architecture Refactor — Headless Audio and Input Drivers

**Phase**: 1 (Headless Game Worker)  
**Priority**: P0  
**Estimated Effort**: 3–5 days  
**Prerequisites**: CMake build system, headless video driver started

## Objective

Create headless audio capture driver (`snd_capture.c`) and input injection driver (`in_inject.c`) that replace hardware dependencies with programmatic APIs for cloud streaming.

## Acceptance Criteria

### Audio
- [ ] `snd_capture.c` provides a fake DMA buffer that the Quake audio mixer writes to
- [ ] `SND_CaptureAudio()` returns valid PCM samples when game sounds are playing
- [ ] Audio format: 16-bit signed PCM, 11025 Hz (Quake native rate)
- [ ] No OSS, ALSA, or waveOut dependencies
- [ ] Audio buffer does not overflow or underflow under normal operation

### Input
- [ ] `in_inject.c` provides queue-based input injection
- [ ] `IN_InjectKeyEvent()` correctly triggers key_down/key_up in engine
- [ ] `IN_InjectMouseEvent()` correctly updates mouse delta and button state
- [ ] Injecting "forward" key causes player movement in captured frames
- [ ] No Win32, X11, or evdev input dependencies

## Implementation Steps

### Audio: snd_capture.c

Based on `snd_null.c` pattern but with a real buffer:

```c
// snd_capture.c — Headless audio capture driver

#include "quakedef.h"
#include "sound.h"

#define CAPTURE_BUFFER_SAMPLES 16384
static int16_t capture_buffer[CAPTURE_BUFFER_SAMPLES];
static volatile int capture_write_pos = 0;

qboolean SNDDMA_Init(void)
{
    shm = &sn;
    shm->speed = 11025;
    shm->samplebits = 16;
    shm->channels = 2;
    shm->samples = CAPTURE_BUFFER_SAMPLES;
    shm->buffer = (unsigned char *)capture_buffer;
    shm->submission_chunk = 512;
    return true;
}

int SNDDMA_GetDMAPos(void)
{
    // Return advancing position — mixer writes here
    return capture_write_pos;
}

void SNDDMA_Shutdown(void)
{
    // Nothing to clean up
}

// Update write position — called after mixer writes
void SNDDMA_Submit(void)
{
    // Advance write position by submission chunk
}

// Export: read captured audio
qboolean SND_CaptureAudio(int16_t **pcm, int *samples, int *rate)
{
    *pcm = capture_buffer;
    *samples = CAPTURE_BUFFER_SAMPLES;
    *rate = 11025;
    return true;
}
```

### Input: in_inject.c

```c
// in_inject.c — Programmatic input injection driver

#include "quakedef.h"

#define INPUT_QUEUE_SIZE 256

typedef struct {
    int key;
    qboolean down;
} key_event_t;

typedef struct {
    int dx, dy;
    int buttons;
} mouse_event_t;

static key_event_t key_queue[INPUT_QUEUE_SIZE];
static int key_queue_head = 0, key_queue_tail = 0;

static mouse_event_t mouse_queue[INPUT_QUEUE_SIZE];
static int mouse_queue_head = 0, mouse_queue_tail = 0;

void IN_Init(void) { }
void IN_Shutdown(void) { }

void IN_Commands(void)
{
    // Dequeue key events, call Key_Event() for each
    while (key_queue_tail != key_queue_head)
    {
        key_event_t *ev = &key_queue[key_queue_tail % INPUT_QUEUE_SIZE];
        Key_Event(ev->key, ev->down);
        key_queue_tail++;
    }
}

void IN_Move(usercmd_t *cmd)
{
    // Dequeue mouse events, update cl.viewangles
    while (mouse_queue_tail != mouse_queue_head)
    {
        mouse_event_t *ev = &mouse_queue[mouse_queue_tail % INPUT_QUEUE_SIZE];
        cl.viewangles[YAW] -= ev->dx * sensitivity.value * 0.022f;
        cl.viewangles[PITCH] += ev->dy * sensitivity.value * 0.022f;
        mouse_queue_tail++;
    }
}

// Export: inject key event from external source (gateway)
void IN_InjectKeyEvent(int key, qboolean down)
{
    int next = (key_queue_head + 1) % INPUT_QUEUE_SIZE;
    if (next != key_queue_tail % INPUT_QUEUE_SIZE)
    {
        key_queue[key_queue_head % INPUT_QUEUE_SIZE].key = key;
        key_queue[key_queue_head % INPUT_QUEUE_SIZE].down = down;
        key_queue_head = next;
    }
}

// Export: inject mouse event from external source (gateway)
void IN_InjectMouseEvent(int dx, int dy, int buttons)
{
    int next = (mouse_queue_head + 1) % INPUT_QUEUE_SIZE;
    if (next != mouse_queue_tail % INPUT_QUEUE_SIZE)
    {
        mouse_queue[mouse_queue_head % INPUT_QUEUE_SIZE].dx = dx;
        mouse_queue[mouse_queue_head % INPUT_QUEUE_SIZE].dy = dy;
        mouse_queue[mouse_queue_head % INPUT_QUEUE_SIZE].buttons = buttons;
        mouse_queue_head = next;
    }
}
```

### CMake Integration

```cmake
if(HEADLESS)
    set(SND_SOURCES snd_capture.c)
    set(IN_SOURCES in_inject.c)
else()
    set(SND_SOURCES snd_linux.c)  # or snd_win.c
    set(IN_SOURCES in_linux.c)    # or in_win.c
endif()
```

## Files to Create

- `WinQuake/snd_capture.c`
- `WinQuake/in_inject.c`

## Files to Modify

- `CMakeLists.txt` — add headless audio/input sources

## Validation

```bash
# Build headless
cmake -B build -DHEADLESS=ON -DNOASM=ON && cmake --build build

# Run with audio test: load map with sounds, capture audio buffer
./build/quake-worker +map e1m1 +playdemo demo1

# Verify: audio buffer contains non-zero data
# Verify: injected keys cause expected game behavior
```

## Risks

- Audio mixer timing: Quake mixer expects DMA position to advance via hardware interrupt. In headless mode, must advance position in sync with frame loop.
- Input queue overflow: if gateway sends faster than engine consumes, events lost. Queue size 256 is generous for 30fps polling.

## Rollback

Remove new files, revert CMakeLists.txt. Original drivers unchanged.
