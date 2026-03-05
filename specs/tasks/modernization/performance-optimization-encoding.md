# Task: Performance Optimization — Encoding Pipeline

**Phase**: 2 (Streaming Gateway)  
**Priority**: P1  
**Estimated Effort**: 3–5 days  
**Prerequisites**: Streaming gateway with basic encoding working

## Objective

Optimize the video and audio encoding pipeline to achieve < 100ms input-to-display latency at 720p@30fps within the CPU budget of 2 vCPU for the gateway container.

## Acceptance Criteria

- [ ] Video encoding time < 15ms per frame at 720p (measured P95)
- [ ] Audio encoding time < 2ms per 20ms frame
- [ ] End-to-end latency (input → display) < 100ms P95
- [ ] CPU utilization for encoding < 180% (of 2 vCPU)
- [ ] Adaptive quality: auto-degrades to 540p if encoding can't keep up
- [ ] No frame drops under sustained 30fps capture rate

## Optimization Strategies

### 1. Video Encoding Tuning

```
FFmpeg parameters for minimum latency:
-c:v libx264
-preset ultrafast
-tune zerolatency
-profile:v baseline
-level 3.1
-g 30                    # keyframe every 1 second
-bf 0                    # no B-frames
-rc cbr                  # constant bitrate
-b:v 3000k              # 3 Mbps target
-maxrate 4000k           # burst allowance
-bufsize 1000k           # small buffer for low latency
-threads 2               # use 2 threads
-sliced-threads 1        # slice-based threading
-intra-refresh 1         # gradual intra refresh
```

### 2. Frame Transfer Optimization

- Shared memory: zero-copy frame transfer from worker to gateway
- Double-buffer: worker writes to buffer A while gateway reads buffer B
- Frame signaling: eventfd or futex to notify gateway of new frame
- Avoid `memcpy`: gateway encoder reads directly from shared buffer

### 3. Adaptive Quality

```
Monitor: encode_time / frame_interval

If encode_time > 25ms (75% of 33ms budget):
  - Drop resolution to 960x540
  - Reduce bitrate to 2 Mbps
  
If encode_time > 30ms (90% of budget):
  - Drop resolution to 640x480
  - Reduce bitrate to 1.5 Mbps
  - Consider dropping to 24fps

If encode_time < 15ms (recovered):
  - Gradually increase resolution back to 720p
  - Increase bitrate
```

### 4. Audio Pipeline

- Resample 11025 Hz → 48000 Hz using libsamplerate (high quality) or linear interpolation (low latency)
- Opus encoding: 20ms frames, 64 kbps mono
- Buffer: accumulate ~900 samples at 11025 Hz (≈81ms of audio) then resample and encode
- Jitter buffer on client-side: 40-60ms

### 5. Pipeline Architecture

```
Frame arrives (shared memory)
  │
  ├─ Convert RGBA → I420 (libyuv)     ~1ms
  │
  ├─ Encode H.264 (libx264)            ~10-15ms
  │
  ├─ Packetize RTP                      ~0.1ms
  │
  └─ Send via WebRTC                    ~0.1ms

Audio arrives (ring buffer, every 20ms)
  │
  ├─ Resample 11025→48000              ~0.2ms
  │
  ├─ Encode Opus                        ~0.5ms
  │
  └─ Send via WebRTC                    ~0.1ms
```

## Validation

### Latency Measurement

1. Instrument frame capture: timestamp when frame leaves worker
2. Instrument encode: measure encode start → end
3. Instrument send: measure RTP packet emission time
4. Client-side: measure decode + render time
5. Round-trip: inject input with timestamp → detect visual response in frame → measure delta

### Benchmark Script

```bash
# Run gateway with profiling
GATEWAY_PROFILE=true docker compose up

# Collect metrics for 60 seconds
curl http://gateway:8443/metrics | grep encode_

# Expected output:
# encode_video_duration_ms_p50: 8
# encode_video_duration_ms_p95: 14
# encode_video_duration_ms_p99: 18
# encode_audio_duration_ms_p50: 0.4
# frames_dropped_total: 0
```

## Risks

- libx264 `ultrafast` quality may produce visible artifacts — acceptable for gaming
- Color space conversion (RGBA → I420) adds latency — use SIMD-optimized libyuv
- WebRTC jitter buffer adds 20-40ms client-side — unavoidable

## Rollback

Encoding parameters are configuration — revert to defaults without code changes.
