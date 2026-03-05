# Risk Analysis

## Risk Assessment Matrix

Risks categorized by likelihood (1–5) × impact (1–5) = risk score.

---

## Technical Risks

### TR-01: Mesa LLVMpipe Performance Insufficient
- **Likelihood**: 3 — Software GL rendering may not sustain 720p@30fps on 2 vCPU
- **Impact**: 4 — Core architecture depends on headless rendering
- **Risk Score**: 12 (HIGH)
- **Description**: LLVMpipe software OpenGL rendering may consume too many CPU cycles, leaving insufficient headroom for the game logic and competing with video encoding for CPU time.
- **Indicators**: Frame time > 33ms, CPU utilization > 95% on worker container
- **Mitigation**: See mitigation-strategies.md TR-01

### TR-02: Video Encoding Latency Too High
- **Likelihood**: 3 — Software H.264 encoding at 720p may exceed latency budget
- **Impact**: 4 — Directly affects player experience (input-to-display latency)
- **Risk Score**: 12 (HIGH)
- **Description**: FFmpeg software encoding of 720p@30fps may exceed the 15ms per-frame budget, especially when competing with rendering for CPU.
- **Indicators**: Encode time > 20ms, E2E latency > 100ms
- **Mitigation**: See mitigation-strategies.md TR-02

### TR-03: Legacy C Code Memory Safety in Cloud
- **Likelihood**: 4 — Known buffer overflows, 25-year-old C code
- **Impact**: 5 — Container escape, RCE, or data breach
- **Risk Score**: 20 (CRITICAL)
- **Description**: Despite `sprintf → snprintf` fixes, 120+ C source files may contain undiscovered memory safety issues. Cloud-exposed services amplify the impact.
- **Indicators**: ASan violations, crashes, unusual memory patterns
- **Mitigation**: See mitigation-strategies.md TR-03

### TR-04: WebRTC Complexity and Browser Compatibility
- **Likelihood**: 3 — WebRTC is notoriously complex (ICE, STUN/TURN, codec negotiation)
- **Impact**: 3 — Players unable to connect from certain browsers or networks
- **Risk Score**: 9 (MEDIUM)
- **Description**: WebRTC negotiation may fail on restrictive networks (corporate firewalls, symmetric NATs). Browser implementations differ.
- **Indicators**: Connection failures > 10%, specific browser error reports
- **Mitigation**: See mitigation-strategies.md TR-04

### TR-05: Container Cold Start Time
- **Likelihood**: 3 — ACA consumption plan may have variable startup times
- **Impact**: 3 — Players wait > 15s to start playing
- **Risk Score**: 9 (MEDIUM)
- **Description**: Game worker containers need to pull image, start process, load game assets, and render first frame. Cold start may exceed acceptable wait time.
- **Indicators**: Session creation time P95 > 15s
- **Mitigation**: See mitigation-strategies.md TR-05

### TR-06: Shared Memory / IPC Between Gateway and Worker
- **Likelihood**: 2 — ACA container groups support shared volumes, but shared memory is less documented
- **Impact**: 4 — If shared memory unavailable, must fall back to socket IPC (higher latency)
- **Risk Score**: 8 (MEDIUM)
- **Description**: The gateway-to-worker frame transfer relies on shared memory for lowest latency. ACA multi-container groups may not support `/dev/shm` or POSIX shared memory.
- **Indicators**: Cannot mount shared memory in ACA container group
- **Mitigation**: See mitigation-strategies.md TR-06

---

## Business Risks

### BR-01: Licensing Compliance for Quake Engine
- **Likelihood**: 2 — WinQuake is GPL-licensed
- **Impact**: 5 — Legal exposure if GPL violated
- **Risk Score**: 10 (HIGH)
- **Description**: WinQuake source is GPL v2. All modifications must be open-source. Cloud service deployment as a hosted service may have implications.
- **Indicators**: Legal review flags
- **Mitigation**: See mitigation-strategies.md BR-01

### BR-02: Game Asset Licensing
- **Likelihood**: 3 — PAK files are proprietary id Software assets
- **Impact**: 4 — Cannot distribute game data without license
- **Risk Score**: 12 (HIGH)
- **Description**: The game engine (GPL) and game data (proprietary) have different licenses. Serving game assets requires legal right to distribute them.
- **Indicators**: Legal review, DMCA takedown
- **Mitigation**: See mitigation-strategies.md BR-02

### BR-03: Cloud Cost Overrun
- **Likelihood**: 3 — Video encoding is CPU-intensive; each session consumes 4+ vCPU
- **Impact**: 3 — Unsustainable operating costs
- **Risk Score**: 9 (MEDIUM)
- **Description**: Each concurrent session requires ~4 vCPU (2 render + 2 encode) + bandwidth. At scale, costs per session may be economically unviable.
- **Indicators**: Cost per session-hour exceeds budget
- **Mitigation**: See mitigation-strategies.md BR-03

---

## Security Risks

### SR-01: Container Escape from Game Worker
- **Likelihood**: 2 — Requires exploiting both engine vulnerability + container runtime
- **Impact**: 5 — Access to host, other containers, Azure infrastructure
- **Risk Score**: 10 (HIGH)
- **Description**: The game worker runs legacy C code that processes untrusted data (maps, mods, demos). A memory corruption exploit could potentially escape container sandbox.
- **Indicators**: Unusual syscalls, network connections from worker
- **Mitigation**: See mitigation-strategies.md SR-01

### SR-02: DDoS on Streaming Infrastructure
- **Likelihood**: 4 — Gaming services are frequent DDoS targets
- **Impact**: 3 — Service unavailability for all players
- **Risk Score**: 12 (HIGH)
- **Description**: WebRTC and WebSocket endpoints are resource-intensive. Amplification attacks or connection floods could exhaust capacity.
- **Indicators**: Sudden traffic spike, connection count anomaly
- **Mitigation**: See mitigation-strategies.md SR-02

### SR-03: Session Hijacking
- **Likelihood**: 2 — Requires intercepting JWT or WebRTC session
- **Impact**: 4 — Attacker controls another player's game session
- **Risk Score**: 8 (MEDIUM)
- **Description**: If session tokens are leaked or WebRTC credentials stolen, an attacker could hijack an active game session.
- **Indicators**: Multiple connections to same session, token reuse from different IPs
- **Mitigation**: See mitigation-strategies.md SR-03

---

## Operational Risks

### OR-01: Team Has No Quake Engine Expertise
- **Likelihood**: 4 — Specialized 1996 C codebase
- **Impact**: 3 — Slow progress, incorrect modifications, introduced bugs
- **Risk Score**: 12 (HIGH)
- **Description**: The WinQuake codebase uses patterns unfamiliar to modern developers (global state, custom memory allocators, setjmp/longjmp, fixed-point math). Modifications may introduce subtle bugs.
- **Indicators**: High bug rate in engine modifications, slow velocity
- **Mitigation**: See mitigation-strategies.md OR-01

### OR-02: Azure Container Apps Limitations
- **Likelihood**: 2 — ACA is production-ready but evolving
- **Impact**: 3 — May need to pivot to AKS or different compute
- **Risk Score**: 6 (LOW)
- **Description**: ACA may lack features needed for this workload (e.g., shared memory, GPU, custom networking). May need to fall back to AKS.
- **Indicators**: Feature requests blocked by ACA constraints
- **Mitigation**: See mitigation-strategies.md OR-02

---

## Risk Heat Map

```
Impact  5 │ TR-03      BR-01
        4 │ TR-01,02   SR-01  BR-02
        3 │ OR-01      TR-04  SR-02  BR-03
        2 │            OR-02  TR-05  TR-06
        1 │
          └──────────────────────────────
            1     2     3     4     5
                    Likelihood
```

## Summary by Priority

| Priority | Risks | Action |
| --- | --- | --- |
| CRITICAL | TR-03 (memory safety) | Immediate: ASan in CI, security audit, sandboxing |
| HIGH | TR-01, TR-02, BR-01, BR-02, SR-01, SR-02, OR-01 | Address in Phase 0–1 planning |
| MEDIUM | TR-04, TR-05, TR-06, BR-03, SR-03 | Address in Phase 2–3 planning |
| LOW | OR-02 | Monitor and plan contingency |
