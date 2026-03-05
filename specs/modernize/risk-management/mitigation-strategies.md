# Mitigation Strategies

## Technical Risk Mitigations

### TR-01: Mesa LLVMpipe Performance Insufficient

**Primary Mitigation**: Performance budget allocation
- Allocate 2 vCPU to worker (rendering + game logic) and 2 vCPU to gateway (encoding)
- Target 720p (1280×720) — reduce to 540p if needed
- Profile LLVMpipe on representative hardware early in Phase 1
- Benchmark: render e1m1 for 60s, measure average frame time

**Secondary Mitigation**: Reduce rendering complexity
- Disable particle effects, reduce lightmap quality
- Use GLQuake simplified path (no multitexture, no transparent water)
- Reduce render resolution, upscale in encoder

**Fallback**: Software renderer path
- If GL path (LLVMpipe) is too slow, fall back to Quake software renderer
- Software renderer is simpler — direct framebuffer output, no GL overhead
- Lower visual quality but more predictable performance

**Escalation**: GPU instances
- Azure Container Apps supports GPU workloads (preview)
- Use GPU for both rendering (hardware GL) and encoding (NVENC)
- Significantly increases cost per session but solves performance

### TR-02: Video Encoding Latency Too High

**Primary Mitigation**: Encoding parameter tuning
- Use `libx264` with `-preset ultrafast -tune zerolatency`
- Target 2-4 Mbps CBR for predictable performance
- Use baseline profile (no B-frames) for minimum latency
- Key frame interval: 1s (30 frames)

**Secondary Mitigation**: Resolution and framerate adaptation
- Adaptive quality: drop to 540p@30fps or 720p@24fps under load
- Skip frames if encoding falls behind (maintain input responsiveness over smoothness)

**Fallback**: Alternative codec
- VP8 (lighter than H.264 on CPU)
- Raw frame streaming over WebSocket (no encoding — requires high bandwidth but zero encode latency)

**Escalation**: Hardware encoding
- GPU instances with NVENC: < 1ms encode latency
- VAAPI on Intel hosts if available in ACA

### TR-03: Legacy C Code Memory Safety in Cloud

**Primary Mitigation**: Static and dynamic analysis
- Build with `-fsanitize=address,undefined` in CI
- Run cppcheck and clang-tidy on all code
- Fix all CRITICAL and HIGH findings from security audit
- Automated ASan test: run game for 5 minutes in CI

**Secondary Mitigation**: Runtime sandboxing
- Seccomp profile restricting dangerous syscalls
- Non-root container with read-only filesystem
- Network policy: worker cannot initiate outbound connections
- AppArmor profile limiting file access to `$QUAKE_BASEDIR` only

**Tertiary Mitigation**: Memory-safe wrapper
- Isolate game engine in sandboxed subprocess (e.g., gVisor, Wasm)
- Communication via shared memory with explicit bounds
- Engine crash contained — does not affect other containers

**Monitoring**: Runtime detection
- Enable Defender for Containers runtime protection
- Monitor for unexpected syscalls, file access, network activity

### TR-04: WebRTC Complexity and Browser Compatibility

**Primary Mitigation**: WebSocket fallback
- Implement dual transport: WebRTC preferred, WebSocket binary fallback
- Client detects WebRTC failure, falls back to WebSocket automatically
- WebSocket: slightly higher latency but works through restrictive firewalls

**Secondary Mitigation**: TURN server
- Deploy Coturn TURN server alongside the platform
- Relay WebRTC media through TURN for symmetric NAT scenarios
- Azure Communication Services as managed TURN alternative

**Testing**: Comprehensive browser matrix
- Test on Chrome, Firefox, Safari, Edge (latest 2 versions)
- Test behind corporate proxy, VPN, symmetric NAT

### TR-05: Container Cold Start Time

**Primary Mitigation**: Pre-warmed pool
- Maintain minimum 2 idle game workers at all times (ACA min replicas)
- Session manager assigns from pool, triggers replacement provisioning

**Secondary Mitigation**: Optimize image and startup
- Minimal container image (< 200 MB)
- Pre-load game assets on shared Azure Files volume (no per-container copy)
- Lazy-load maps: start `Host_Init()` immediately, load map data asynchronously

**Tertiary Mitigation**: Session queuing
- If no workers available, queue session request
- Show loading screen with estimated wait time
- Game starts when worker ready (< 15s target)

### TR-06: Shared Memory / IPC Between Gateway and Worker

**Primary Mitigation**: Validate ACA container group shared volumes
- Test `emptyDir` volume mount in ACA multi-container group
- Test shared `/dev/shm` support
- Benchmark: shared memory vs. Unix socket vs. localhost TCP

**Fallback 1**: Unix domain socket
- Mount shared volume, use Unix socket for frame transfer
- Marginal latency increase (~0.1ms added) vs. shared memory

**Fallback 2**: Localhost TCP
- Gateway connects to worker on `localhost:PORT`
- Worker sends raw frames, gateway reads
- Slightly higher latency and CPU overhead (~0.5ms)

**Fallback 3**: Sidecar in same container
- Build gateway and worker into single container image
- Communicate via in-process shared memory
- Lose independent scaling but simplifies IPC

---

## Business Risk Mitigations

### BR-01: Licensing Compliance for Quake Engine

**Mitigation**: GPL compliance review
- WinQuake is GPL v2 — all source modifications must be published
- Host the modified engine source on public GitHub repository
- Document all modifications clearly
- GPL applies to the engine code, not the service infrastructure
- New services (gateway, session-manager, etc.) are separate programs communicating via IPC — not derivative works under GPL

**Legal advice**: Consult legal counsel on GPL implications for cloud-hosted services.

### BR-02: Game Asset Licensing

**Mitigation**: Use shareware assets
- Quake shareware episode (Episode 1) is freely distributable
- Deploy `pak0.pak` from shareware release — no licensing issue
- For full game: require users to supply their own `pak1.pak` (upload or link to purchase)

**Alternative**: Open-source game data
- LibreQuake project provides GPL-licensed replacement game data
- Lower visual quality but fully open-source

### BR-03: Cloud Cost Overrun

**Primary Mitigation**: Cost modeling
- Model: 4 vCPU + 1 GB RAM per session × $/vCPU-hour × average session duration
- Set auto-scaling maximum to cap costs
- Use consumption plan (pay per use, not reserved capacity)

**Secondary Mitigation**: Spot/Preemptible instances
- Use Azure Spot for non-production environments
- Evaluate ACA with spot node pools

**Cost controls**:
- Azure Cost Management budgets with alerts at 80% and 100%
- Auto-shutdown idle sessions after 15 minutes
- Daily cost reports to operations team

---

## Security Risk Mitigations

### SR-01: Container Escape from Game Worker

**Mitigation: Defense in depth**
1. Non-root container execution
2. Read-only filesystem
3. Seccomp profile (restrict syscalls)
4. Network isolation: worker in private VNet, no outbound internet
5. No privileged capabilities (`--cap-drop=ALL`)
6. Defender for Containers runtime monitoring
7. If available: gVisor sandbox for stronger isolation

### SR-02: DDoS on Streaming Infrastructure

**Mitigation: Multi-layer DDoS protection**
1. Azure DDoS Network Protection on VNet
2. Azure Front Door with rate limiting (100 req/min per IP for APIs)
3. WebSocket connection limits per IP (max 5 concurrent)
4. WebRTC ICE rate limiting
5. Session creation throttle (max 10 sessions/min per user)
6. Geographic allowlisting (optional: restrict to target regions)

### SR-03: Session Hijacking

**Mitigation: Token and session security**
1. Short-lived JWT tokens (15-minute expiry) with refresh tokens
2. Session bound to token `sub` claim — cannot transfer
3. WebRTC DTLS fingerprint pinning
4. Detect concurrent connections to same session → kill oldest
5. Log and alert on token reuse from different IP addresses

---

## Operational Risk Mitigations

### OR-01: Team Has No Quake Engine Expertise

**Mitigation: Knowledge transfer**
1. Use reverse engineering documentation (`specs/docs/`, `specs/features/`) as onboarding
2. Minimize engine modifications — strangler fig approach wraps, doesn't rewrite
3. Create decision log for all engine changes with rationale
4. Pair programming: one person familiar with C systems + one with cloud
5. AddressSanitizer catches mistakes in engine modifications early

### OR-02: Azure Container Apps Limitations

**Mitigation: Validate early, plan contingency**
1. Proof of concept in Phase 1: validate container group, shared volumes, networking
2. Document ACA constraints hit during implementation
3. Contingency: migrate to Azure Kubernetes Service (AKS) if ACA insufficient
4. Architecture is container-based — migration is re-deployment, not rewrite
5. Bicep IaC can be adapted from ACA to AKS modules
