# Testing Strategy

## Testing Principles

1. **Feature preservation**: Every existing feature documented in `specs/features/` must have validation tests
2. **Progressive confidence**: Tests run at every level — commit, PR, deployment
3. **Automated gates**: No deployment without passing tests
4. **Performance baselines**: Track latency and throughput regressions

## Test Levels

### Level 1: Unit Tests (C Code)

**Framework**: CMocka or custom minimal test harness (Quake has no test framework)

**Scope**: Pure functions and isolated logic in the engine

| Module | Test Focus | Priority |
| --- | --- | --- |
| `common.c` | `COM_Parse`, path sanitization, byte swapping | P0 |
| `cmd.c` | Command parsing, tokenization, alias expansion | P1 |
| `cvar.c` | Cvar get/set, type conversion, clamping | P1 |
| `crc.c` | CRC calculation correctness | P1 |
| `mathlib.c` | Vector ops, angle normalization, trig tables | P1 |
| `zone.c` | `Z_Malloc`/`Z_Free` — no overflow, no double-free | P0 |
| `snd_mix.c` | Audio mixing arithmetic (overflow, clipping) | P2 |
| `in_inject.c` | Event queue: enqueue, dequeue, overflow behavior | P0 |
| `vid_headless.c` | Frame capture: correct dimensions, non-zero output | P0 |
| `snd_capture.c` | Audio ring buffer: read/write, wrap-around | P0 |

**Coverage Target**: 60% of new/modified code, critical paths 90%+

**CI Gate**: Must pass on every commit. Build fails if any unit test fails.

### Level 2: Integration Tests (Container Level)

**Framework**: Docker Compose + pytest (or Go test client)

**Scope**: Service-to-service interactions

| Test | Description | Priority |
| --- | --- | --- |
| Worker startup | Container starts, `/healthz` returns 200 within 10s | P0 |
| Worker shutdown | SIGTERM → clean exit (code 0) within 5s | P0 |
| Frame capture | Worker produces ≥1 non-zero frame within 5s of start | P0 |
| Audio capture | Worker produces non-zero audio within 5s of start | P1 |
| Input injection | Inject movement key → player position changes in subsequent frames | P0 |
| Worker + Gateway | Gateway connects to worker, produces encoded video | P0 |
| Gateway WebRTC | Browser test client completes WebRTC negotiation, receives video track | P1 |
| Session lifecycle | Create session → worker starts → destroy session → worker stops | P0 |

**Environment**: Docker Compose locally; dedicated ACA dev environment in CI.

**CI Gate**: Must pass before merge to `main`.

### Level 3: End-to-End Tests (Browser)

**Framework**: Playwright + custom WebRTC client

**Scope**: Full user journey through browser

| Test | Description | Priority |
| --- | --- | --- |
| Login flow | OAuth redirect → token → session creation | P0 |
| Session creation | Create session → receive WebSocket URL | P0 |
| Stream playback | Browser receives video frames (> 0 fps) | P0 |
| Audio playback | Browser receives audio (non-silent) | P1 |
| Input round-trip | Press key → see effect in video within 200ms | P0 |
| Session end | Close browser → session cleaned up within 30s | P1 |
| Multiple sessions | 5 concurrent sessions, all streaming | P1 |
| Reconnection | Disconnect WebRTC → reconnect → resume stream | P2 |

**Environment**: Staging ACA environment with headless Chrome.

**CI Gate**: Must pass before production deployment.

### Level 4: Performance Tests

**Framework**: k6 (session creation API), custom latency measurement tool

**Scope**: Performance SLOs under load

| Metric | Target | Test Method |
| --- | --- | --- |
| Input-to-display latency | < 100ms P95 | Inject timestamped input → measure frame with visual response |
| Frame rate | ≥ 30 fps sustained | Measure frame delivery rate over 60s |
| Video encoding time | < 15ms per frame | Instrument FFmpeg encode step |
| Session creation time | < 5s (warm), < 15s (cold) | API call to first video frame |
| API response time | < 200ms P95 | k6 load test on session-manager API |
| Concurrent sessions | 50 per node | Scale test: ramp to target, monitor stability |
| Memory stability | No growth over 30 min | Monitor container memory over extended run |

**Environment**: Staging ACA environment, production-like scale.

**Frequency**: Weekly, and before every production release.

### Level 5: Security Tests

| Test | Tool | Scope | Frequency |
| --- | --- | --- | --- |
| Container scan | Trivy | All container images in ACR | Every build (CI) |
| SAST | cppcheck + clang-tidy | C source code | Every build (CI) |
| DAST | OWASP ZAP | Session Manager API, Assets API | Weekly + pre-release |
| Dependency scan | Trivy, Dependabot | All service dependencies | Daily |
| Penetration test | Manual / contracted | Full platform | Quarterly |

## Feature Preservation Validation

Each feature from `specs/features/` maps to validation tests:

| Feature | Validation Method |
| --- | --- |
| Rendering | Visual diff: captured frames match reference screenshots (SSIM > 0.95) |
| Physics/movement | Inject movement sequence → verify player coordinates match expected trajectory |
| Sound | Captured audio contains expected sound events (frequency analysis) |
| Console | Inject console commands → verify cvar changes and command execution |
| HUD/status bar | Visual diff of HUD region in captured frames |
| Save/Load | Save game → restart worker → load game → verify player position restored |
| Demo recording | Record demo → play back → frame-compare with original |
| Multiplayer | Two workers connected → actions in one visible in other's stream |
| Mod support | Load known mod → verify mod-specific content visible |
| QuakeC | Trigger QuakeC logic → verify game behavior changes |

## Test Data

| Data | Storage | Purpose |
| --- | --- | --- |
| id1/pak0.pak | Azure Files (shared) | Base game assets for all tests |
| Reference frames | Git LFS in test repo | Visual regression baselines |
| Known-good demo files | Git LFS | Demo playback regression |
| Test save files | Git LFS | Save/load validation |
| Input sequences | JSON in test repo | Reproducible input scripts |

## Regression Strategy

- **Automated**: All Level 1–3 tests run on every PR and merge to main
- **Visual regression**: Frame comparison with ≥0.95 SSIM against baselines; failures require manual review
- **Performance regression**: P95 latency increase > 20% from baseline triggers alert
- **Baseline updates**: Intentional visual changes require baseline update with PR review
