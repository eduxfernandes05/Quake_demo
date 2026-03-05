# Task: Architecture Refactor — Streaming Gateway Service

**Phase**: 2 (Streaming Gateway)  
**Priority**: P0  
**Estimated Effort**: 10–15 days  
**Prerequisites**: Headless game worker running in container with frame/audio/input APIs

## Objective

Build a new streaming gateway service that reads frames and audio from the game worker, encodes to H.264/Opus, streams to browsers via WebRTC, and receives player input.

## Acceptance Criteria

- [ ] Gateway connects to co-located game worker via shared memory or Unix socket
- [ ] Video: encodes RGBA frames to H.264 baseline profile at 720p@30fps
- [ ] Audio: encodes PCM to Opus at 48kHz (upsampled from 11025Hz)
- [ ] WebRTC: browser connects, receives video + audio tracks
- [ ] WebRTC data channel: browser sends keyboard/mouse events
- [ ] Input round-trip: key press in browser → visible in stream within 100ms
- [ ] Signaling: WebSocket endpoint for WebRTC session establishment
- [ ] TLS on WebSocket signaling endpoint
- [ ] Adaptive: reduces quality under CPU pressure
- [ ] Containerized and deployable to ACA alongside worker

## Architecture

```
Browser ←→ [WebRTC / WebSocket] ←→ Streaming Gateway ←→ Game Worker
                                        │
                                  ┌─────┴─────┐
                                  │ FFmpeg     │
                                  │ encode     │
                                  │ H.264+Opus │
                                  └────────────┘
```

## Implementation Steps

### 1. Project Scaffold (Go preferred)

```
streaming-gateway/
├── cmd/
│   └── gateway/
│       └── main.go
├── internal/
│   ├── worker/        # Game worker IPC
│   │   ├── frames.go  # Frame capture reader
│   │   └── input.go   # Input injection writer
│   ├── encoder/       # Video/audio encoding
│   │   ├── video.go   # FFmpeg H.264 encoding
│   │   └── audio.go   # Opus encoding
│   ├── webrtc/        # WebRTC transport
│   │   ├── peer.go    # Peer connection management
│   │   └── signal.go  # Signaling WebSocket
│   └── server/        # HTTP server
│       └── server.go  # Health + signaling endpoints
├── Dockerfile
├── go.mod
└── go.sum
```

### 2. Worker IPC — Frame Reader

Read RGBA frames from game worker:
- Option A: Shared memory (`/dev/shm/quake-frames`) — lowest latency
- Option B: Unix domain socket — reliable, slightly higher latency
- Option C: localhost TCP — simplest, highest latency

Frame protocol:
```
[4 bytes: width][4 bytes: height][4 bytes: frame_num][width*height*4 bytes: RGBA]
```

### 3. Video Encoding

Use FFmpeg C API (via CGo) or shell out to `ffmpeg`:
- Input: raw RGBA frames
- Output: H.264 NAL units
- Settings: `-preset ultrafast -tune zerolatency -profile baseline`
- Bitrate: 2-4 Mbps CBR
- No B-frames (minimize latency)
- Key frame every 30 frames (1 second)

### 4. Audio Encoding

- Input: 16-bit PCM at 11025 Hz from worker
- Resample to 48000 Hz (Opus requirement)
- Encode with libopus: 64 kbps, mono, 20ms frames
- Output: Opus packets

### 5. WebRTC Transport

Use Pion WebRTC (Go library):
- Create PeerConnection per browser session
- Add video track (H.264 RTP)
- Add audio track (Opus RTP)
- Add data channel for input events
- Handle ICE, DTLS, SRTP automatically

### 6. Signaling WebSocket

```
GET /ws/signal?session={id}
→ WebSocket upgrade
← { type: "offer", sdp: "..." }
→ { type: "answer", sdp: "..." }
← { type: "candidate", candidate: "..." }
→ { type: "candidate", candidate: "..." }
```

### 7. Input Forwarding

Browser sends via data channel:
```json
{ "type": "keydown", "key": 87 }    // W
{ "type": "keyup", "key": 87 }
{ "type": "mousemove", "dx": 5, "dy": -3 }
{ "type": "mousedown", "button": 0 }
```

Gateway translates to worker input injection API calls.

### 8. Browser Client

```html
<!-- Minimal browser client -->
<video id="game" autoplay playsinline></video>
<script>
// WebSocket signaling → WebRTC negotiation
// Receive video + audio tracks → display
// Capture keyboard + mouse → send via data channel
// Pointer lock for FPS controls
</script>
```

### 9. Containerization

```dockerfile
FROM golang:1.22 AS builder
WORKDIR /app
COPY go.* ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=1 go build -o gateway ./cmd/gateway/

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y libavcodec-dev libopus-dev && rm -rf /var/lib/apt/lists/*
RUN useradd -r -s /usr/sbin/nologin gateway
COPY --from=builder /app/gateway /usr/local/bin/
USER gateway
EXPOSE 8443
HEALTHCHECK CMD curl -sf http://localhost:8443/healthz || exit 1
ENTRYPOINT ["gateway"]
```

## Key Libraries

| Library | Purpose |
| --- | --- |
| pion/webrtc (Go) | WebRTC implementation |
| FFmpeg (libavcodec) | H.264 video encoding |
| libopus | Opus audio encoding |
| gorilla/websocket | WebSocket signaling |

## Validation

1. Start worker + gateway in Docker Compose
2. Open browser at `https://localhost:8443`
3. Complete WebRTC negotiation
4. See game video rendering
5. Hear game audio
6. Press WASD → player moves → visible in stream
7. Mouse look → view rotates → visible in stream
8. Measure: key press → visual response < 100ms

## Risks

- WebRTC complexity: ICE negotiation, codec negotiation, NAT traversal
- FFmpeg CGo bindings can be fragile — consider shell-out alternative
- Latency budget: frame capture (1ms) + encode (15ms) + network (20ms) + decode (5ms) = 41ms best case

## Rollback

Gateway is entirely new code, independent service. Remove without affecting game worker.
