# Validation Criteria

## Phase Gate Criteria

Each phase must meet all success criteria before progressing to the next.

---

## Phase 0: Foundation — Exit Criteria

| # | Criterion | Validation Method | Pass Condition |
| --- | --- | --- | --- |
| 0.1 | CMake builds on Linux | `cmake -B build && cmake --build build` | Exit code 0, binary produced |
| 0.2 | No x86 assembly | `grep -r '\.s$' CMakeLists.txt` | No assembly files in build |
| 0.3 | No sprintf | `grep -rn '\bsprintf\b' WinQuake/*.c` | Zero matches |
| 0.4 | CI passes | GitHub Actions workflow on main | Green badge |
| 0.5 | Docker builds | `docker build -t quake-worker .` | Exit code 0, image < 500 MB |
| 0.6 | Address sanitizer clean | Build with ASan, run for 60s | No ASan violations |
| 0.7 | Bicep deploys | `az deployment group create` | All resources provisioned |

---

## Phase 1: Headless Game Worker — Exit Criteria

| # | Criterion | Validation Method | Pass Condition |
| --- | --- | --- | --- |
| 1.1 | Headless startup | Container starts without display | No X11/display errors in logs |
| 1.2 | Frame production | Capture 100 frames from running game | ≥95 frames non-zero, correct dimensions |
| 1.3 | Visual correctness | Compare captured frame to reference | SSIM ≥ 0.90 against reference screenshot |
| 1.4 | Audio production | Capture 5s audio from game with sounds playing | Non-zero PCM samples, correct sample rate |
| 1.5 | Input injection | Inject "forward" key for 2s → capture frames | Player position changed (frame diff > threshold) |
| 1.6 | Health check | `curl http://localhost:8080/healthz` | HTTP 200 within 2s of game loop start |
| 1.7 | Graceful shutdown | `docker stop <container>` | Exit code 0 within 5s |
| 1.8 | Config from env | Set `QUAKE_MAP=e1m1` → capture | Correct map loaded (visual verification) |
| 1.9 | Structured logging | Parse stdout as JSON | Valid JSON lines with timestamp, level, message |
| 1.10 | Memory stability | Run for 30 minutes, monitor RSS | Memory growth < 10 MB over baseline |

---

## Phase 2: Streaming Gateway — Exit Criteria

| # | Criterion | Validation Method | Pass Condition |
| --- | --- | --- | --- |
| 2.1 | WebRTC connection | Browser connects to gateway signaling | ICE connected, DTLS handshake complete |
| 2.2 | Video stream received | Browser WebRTC stats | Frames decoded > 0, framerate ≥ 25 fps |
| 2.3 | Audio stream received | Browser WebRTC stats | Audio packets received > 0, no underflow |
| 2.4 | Input works | Press W in browser → player moves | Visual change in stream within 200ms |
| 2.5 | Mouse look | Move mouse in browser → view rotates | View angle change visible in stream |
| 2.6 | Encoding latency | Instrument FFmpeg encode timing | < 20ms per frame at 720p |
| 2.7 | E2E latency | Timestamped input → visual response | < 100ms P95 |
| 2.8 | Stream quality | Visual inspection + VMAF | VMAF ≥ 80 at 2 Mbps |
| 2.9 | TLS on signaling | Verify TLS handshake on WebSocket | TLS 1.3, valid certificate |
| 2.10 | Cross-browser | Test on Chrome, Firefox, Edge | All three connect and play |

---

## Phase 3: Session Manager — Exit Criteria

| # | Criterion | Validation Method | Pass Condition |
| --- | --- | --- | --- |
| 3.1 | Auth required | Call API without token | HTTP 401 |
| 3.2 | Auth works | Login via Entra ID → call API with token | HTTP 200 + session created |
| 3.3 | Session creates worker | Create session → poll → worker running | Worker `/healthz` returns 200 |
| 3.4 | Session returns connection | Create session response | Contains `websocketUrl` for gateway |
| 3.5 | Session cleanup | Delete session → worker removed | Worker container stopped within 30s |
| 3.6 | Concurrent sessions | Create 10 sessions simultaneously | All 10 receive unique connection URLs |
| 3.7 | Session isolation | Two sessions → actions in one invisible in other | Frames differ between sessions |
| 3.8 | Capacity limits | Create sessions beyond capacity | HTTP 429 or 503 when full |
| 3.9 | Session ownership | User A tries to delete User B's session | HTTP 403 |
| 3.10 | Idle timeout | Leave session unattended for 15 min | Session auto-cleaned |

---

## Phase 4: Supporting Services — Exit Criteria

| # | Criterion | Validation Method | Pass Condition |
| --- | --- | --- | --- |
| 4.1 | Assets API serves maps | `GET /api/assets/maps` | JSON array of available maps |
| 4.2 | Asset delivery via CDN | Request asset, check response headers | `x-cache: HIT` from CDN on second request |
| 4.3 | Telemetry ingestion | POST event → query App Insights | Event appears within 5 minutes |
| 4.4 | Distributed traces | Create session → play → end | Single trace spans all services in App Insights |
| 4.5 | Dashboards operational | Open Azure Monitor Workbook | All panels render with data |
| 4.6 | Alerts configured | Trigger test alert condition | Notification received |

---

## Phase 5: Production Hardening — Exit Criteria

| # | Criterion | Validation Method | Pass Condition |
| --- | --- | --- | --- |
| 5.1 | WAF active | Attempt SQL injection via API | Request blocked, WAF log entry |
| 5.2 | Container scan clean | Trivy scan of all images | No CRITICAL or HIGH CVEs |
| 5.3 | Pen test passed | Third-party penetration test report | No CRITICAL findings; HIGH remediation plan |
| 5.4 | Load test passed | 100 concurrent sessions for 30 min | P95 latency < 100ms, no crashes |
| 5.5 | Canary deploy works | Deploy new revision at 10% traffic | Metrics visible; rollback works |
| 5.6 | Runbooks exist | Review runbook documents | Cover: incident, scaling, rollback, cost |
| 5.7 | Cost alerts set | Check Azure Cost Management | Budget alerts at 80% and 100% |
| 5.8 | Compliance review | Security checklist sign-off | All items addressed or accepted with justification |

---

## Ongoing Operational SLOs

| SLO | Target | Measurement | Alert Threshold |
| --- | --- | --- | --- |
| Availability | 99.5% monthly | Successful `/healthz` checks / total checks | < 99% over 1 hour |
| Session creation success rate | 99% | Successful creates / total create requests | < 95% over 15 min |
| Input-to-display latency | < 100ms P95 | Client-measured RTT | > 150ms P95 over 5 min |
| Frame rate | ≥ 28 fps average | Client-measured decode rate | < 25 fps over 1 min |
| Session creation time | < 10s P95 | API call to first video frame | > 15s P95 over 5 min |
| Error rate | < 1% | 5xx responses / total responses | > 5% over 5 min |
