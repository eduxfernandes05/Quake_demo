# Feature: Sound System

## Feature ID
`sound-system`

## Purpose
Provide real-time spatial audio for sound effects and ambient sounds, plus CD audio music playback. The sound system mixes multiple channels with 3D spatialization (volume and stereo panning based on listener position).

## Scope
- Multi-channel sound mixing (up to 128 channels)
- 3D spatial audio (distance attenuation and stereo panning)
- Ambient sounds (water, wind, sky) that vary with player location
- Sound precaching and WAV file loading
- DMA-based streaming audio output
- CD audio track playback for music
- Platform-abstracted audio drivers

## User Workflows

### Sound Effect Playback
1. Game logic (QuakeC or engine) calls `S_StartSound(entity, channel, sfx, origin, volume, attenuation)`
2. Sound system finds a free mixing channel (or overrides lowest-priority one)
3. WAV data is loaded from PAK/filesystem (cached in memory after first load)
4. Each frame, `S_Update()` recalculates volume and stereo pan based on listener position/orientation relative to sound origin
5. `S_PaintChannels()` mixes all active channels into the DMA buffer
6. Hardware plays the mixed audio from DMA buffer continuously

### Ambient Sound
1. Engine identifies which BSP leaf the player occupies
2. Each leaf has ambient sound levels for 4 types: water, sky, slime, lava
3. Ambient channels fade toward the target levels ($ambient_level, $ambient_fade)
4. Creates persistent environmental soundscape

### CD Audio Music
1. Map trigger or console command initiates `CDAudio_Play(track, looping)`
2. Platform CD driver sends play command to CD-ROM
3. CD audio plays through hardware independently of sound mixer
4. Track changes on level transitions (triggered by QuakeC)

## Functional Requirements

### FR-SND-01: Channel Management
- 8 channels per entity (0-7), channel 0 = auto-assign
- Static sounds (ambient point sources) use separate allocation from `MAX_CHANNELS`
- Channel priority: higher-volume sounds override lower
- `S_StopSound(entity, channel)` immediately stops a sound

### FR-SND-02: Spatial Audio
- Distance attenuation: `ATTN_NONE` (0), `ATTN_NORM` (1), `ATTN_IDLE` (2), `ATTN_STATIC` (3)
- Nominal clip distance: 1000 units
- Stereo panning based on dot product of listener-right vector and sound direction
- No HRTF or reverb processing

### FR-SND-03: WAV Loading
- Supports 8-bit and 16-bit PCM WAV files
- Resamples to device output rate
- `loadas8bit` cvar forces 16→8 bit conversion for memory savings
- Sound precaching during level load (`precache_sound` in QuakeC)
- MAX_SFX: 512 unique sounds

### FR-SND-04: DMA Mixing
- Ring buffer written ahead of DMA read position (`_snd_mixahead` cvar, default 0.1s)
- 8-bit or 16-bit output
- Mono or stereo output
- Sample rates: typically 11025 Hz (configurable)
- `S_PaintChannels` does the actual mixing with volume scaling

### FR-SND-05: Ambient Sounds
- 4 ambient sound types per BSP leaf (water, sky, slime, lava)
- Levels specified in BSP data per leaf
- Smooth fade transitions controlled by `ambient_fade` cvar (units/sec)
- Maximum volume clamped by `ambient_level` cvar

### FR-SND-06: CD Audio
- Track play (one-shot or looping)
- Pause, resume, stop, eject commands
- Auto-resume after pause
- Volume control independent of sound effects
- `bgmvolume` cvar (0.0–1.0)
- Console commands: `cd play <track>`, `cd loop <track>`, `cd stop`, `cd pause`, `cd resume`, `cd eject`, `cd info`

## Implementation Files

### Sound Core
| File | Purpose |
|------|---------|
| `snd_dma.c` | Main sound control: init, update, channel management, spatialization |
| `snd_mix.c` | Audio mixing and painting channels into DMA buffer |
| `snd_mem.c` | WAV file loading and resampling |
| `sound.h` | Sound types: `sfx_t`, `channel_t`, `dma_t`, `sfxcache_t` |

### Platform Drivers
| File | Purpose |
|------|---------|
| `snd_win.c` | Windows: DirectSound + waveOut fallback |
| `snd_linux.c` | Linux: OSS (`/dev/dsp`) |
| `snd_sun.c` | Solaris: Sun audio |
| `snd_null.c` | Null driver (no sound) |
| `snd_dos.c` | DOS: DMA sound |
| `snd_gus.c` | DOS: Gravis UltraSound |

### CD Audio
| File | Purpose |
|------|---------|
| `cd_audio.c` | Platform-independent CD audio wrapper |
| `cd_win.c` | Windows: MCI CD audio |
| `cd_linux.c` | Linux: ioctl CD audio |
| `cd_null.c` | Null CD driver |
| `cdaudio.h` | CD audio interface |

### Assembly Optimizations
| File | Purpose |
|------|---------|
| `snd_mixa.s` | x86 assembly mixing routines |

## Configuration (CVars)
| CVar | Default | Description |
|------|---------|-------------|
| `volume` | 0.7 | Sound effects volume |
| `bgmvolume` | 1.0 | CD music volume |
| `nosound` | 0 | Disable all sound |
| `precache` | 1 | Precache sounds during level load |
| `loadas8bit` | 0 | Force 8-bit sound loading |
| `ambient_level` | 0.3 | Maximum ambient volume |
| `ambient_fade` | 100 | Ambient fade speed (units/sec) |
| `snd_noextraupdate` | 0 | Skip extra sound updates |
| `snd_show` | 0 | Show active channels (debug) |
| `_snd_mixahead` | 0.1 | Mix-ahead buffer time (seconds) |
| `bgmbuffer` | 4096 | Background music buffer size |

## Dependencies
- **Common/Filesystem** (`common.c`): WAV file loading from PAK
- **Zone Memory** (`zone.c`): Sound cache allocation (Cache_Alloc)
- **BSP Model** (`model.c`): Ambient sound levels per leaf
- **Platform** (`sys_*.c`): DMA buffer access, timing

## Acceptance Criteria
- AC-01: Sound effects play with correct spatial positioning (stereo panning)
- AC-02: Multiple sounds mix without clipping or distortion
- AC-03: Ambient sounds transition smoothly when entering different BSP areas
- AC-04: CD audio plays the correct track per level
- AC-05: Sound precaching completes during level load without hitches
- AC-06: No audible gaps or pops in DMA buffer playback
- AC-07: `nosound 1` disables all audio without errors
