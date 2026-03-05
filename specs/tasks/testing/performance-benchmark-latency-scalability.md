# Task: Performance Benchmark — Latency and Scalability

**Phase**: 2–5 (Ongoing from Streaming Gateway onward)  
**Priority**: P1  
**Estimated Effort**: 3–5 days initial setup, ongoing refinement  
**Prerequisites**: Streaming gateway + game worker running

## Objective

Establish automated performance benchmarks that measure input-to-display latency, encoding throughput, and session density, with baselines tracked over time.

## Acceptance Criteria

- [ ] Latency benchmark: measures end-to-end input-to-display latency
- [ ] Throughput benchmark: measures max concurrent sessions per node
- [ ] Encoding benchmark: measures video encode time per frame
- [ ] Memory stability: verifies no memory growth over 30-minute runs
- [ ] Automated: runs weekly in staging + before every production deploy
- [ ] Baseline tracking: results stored and compared to previous runs
- [ ] Alert: P95 latency increase > 20% from baseline triggers failure

## Performance Metrics

| Metric | Target | Measurement Method |
| --- | --- | --- |
| Input-to-display latency | < 100ms P95 | Timestamped input injection + frame analysis |
| Frame rate | ≥ 30 fps sustained | Frame counter over 60s |
| Video encode time | < 15ms P95 per frame | Gateway instrumentation |
| Audio encode time | < 2ms per 20ms frame | Gateway instrumentation |
| Session creation time (warm) | < 5s | API call to first video frame |
| Session creation time (cold) | < 15s | API call to first video frame (from 0 replicas) |
| Memory usage (worker) | < 512 MB stable | Container metrics over 30 min |
| Memory usage (gateway) | < 256 MB stable | Container metrics over 30 min |
| Concurrent sessions per node | ≥ 8 (16 vCPU node) | Scale test |

## Benchmark Tests

### 1. Latency Benchmark

```python
# benchmark_latency.py
"""
Measures input-to-display latency by:
1. Injecting a known input at timestamp T1
2. Analyzing video frames for visible effect
3. Computing T2 - T1
"""

import time
import websocket

def measure_latency(session_url, iterations=100):
    latencies = []
    ws = websocket.WebSocket()
    ws.connect(session_url)
    
    for i in range(iterations):
        t1 = time.monotonic_ns()
        # Send key press (triggers visible change)
        ws.send(json.dumps({"type": "keydown", "key": 87}))  # W
        
        # Wait for frame with visible movement
        frame = wait_for_changed_frame(ws, timeout_ms=500)
        t2 = time.monotonic_ns()
        
        ws.send(json.dumps({"type": "keyup", "key": 87}))
        latencies.append((t2 - t1) / 1_000_000)  # Convert to ms
        time.sleep(0.1)
    
    p50 = sorted(latencies)[len(latencies) // 2]
    p95 = sorted(latencies)[int(len(latencies) * 0.95)]
    p99 = sorted(latencies)[int(len(latencies) * 0.99)]
    
    return {"p50_ms": p50, "p95_ms": p95, "p99_ms": p99}
```

### 2. Session Density Benchmark

```bash
#!/bin/bash
# benchmark_density.sh — Find max sessions per node

MAX_SESSIONS=20
ACTIVE=0
RESULTS=()

for i in $(seq 1 $MAX_SESSIONS); do
    # Create session
    SESSION=$(curl -s -X POST "$API_URL/api/sessions" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d '{"map":"e1m1"}' | jq -r .id)
    
    # Wait for ready
    wait_for_session_ready "$SESSION" 15
    
    # Check all sessions healthy
    ALL_HEALTHY=true
    for s in "${SESSIONS[@]}"; do
        FPS=$(get_session_fps "$s")
        if (( $(echo "$FPS < 25" | bc -l) )); then
            ALL_HEALTHY=false
            break
        fi
    done
    
    if ! $ALL_HEALTHY; then
        echo "Density limit reached at $ACTIVE sessions"
        break
    fi
    
    ACTIVE=$i
    SESSIONS+=("$SESSION")
done

echo "Max concurrent sessions: $ACTIVE"

# Cleanup
for s in "${SESSIONS[@]}"; do
    curl -s -X DELETE "$API_URL/api/sessions/$s" -H "Authorization: Bearer $TOKEN"
done
```

### 3. Memory Stability Benchmark

```bash
#!/bin/bash
# benchmark_memory.sh — Monitor memory over 30 minutes

SESSION=$(create_session "e1m1")
wait_for_session_ready "$SESSION"

INITIAL_RSS=$(get_container_memory "$SESSION")
echo "Initial RSS: ${INITIAL_RSS} MB"

for i in $(seq 1 30); do
    sleep 60
    CURRENT_RSS=$(get_container_memory "$SESSION")
    GROWTH=$((CURRENT_RSS - INITIAL_RSS))
    echo "Minute $i: RSS=${CURRENT_RSS}MB, Growth=${GROWTH}MB"
    
    if (( GROWTH > 50 )); then
        echo "FAIL: Memory growth exceeds 50MB"
        destroy_session "$SESSION"
        exit 1
    fi
done

echo "PASS: Memory stable over 30 minutes"
destroy_session "$SESSION"
```

## CI Configuration

```yaml
# .github/workflows/performance.yml
name: Performance Benchmarks
on:
  schedule:
    - cron: '0 6 * * 1'  # Weekly Monday 6 AM
  workflow_dispatch:
  push:
    branches: [main]
    
jobs:
  benchmark:
    runs-on: ubuntu-latest
    environment: staging
    steps:
      - uses: actions/checkout@v4
      - run: pip install websocket-client numpy
      - run: python benchmarks/benchmark_latency.py --url $STAGING_URL --output results.json
      - run: python benchmarks/compare_baseline.py results.json baseline.json
      - uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: results.json
```

## Baseline Management

- Store baseline results in `benchmarks/baselines/` (JSON)
- Compare each run against baseline: fail if P95 regresses > 20%
- Update baseline after intentional performance changes (with PR review)

## Risks

- Benchmark variability: cloud VMs have variable performance. Run 3x and take median.
- Staging vs. production: performance may differ. Establish separate baselines.

## Rollback

Benchmarks are read-only. No impact on production.
