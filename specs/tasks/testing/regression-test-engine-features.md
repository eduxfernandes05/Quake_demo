# Task: Regression Testing — Engine Feature Preservation

**Phase**: 1 (Headless Game Worker)  
**Priority**: P0  
**Estimated Effort**: 5–7 days  
**Prerequisites**: Headless game worker with frame capture working

## Objective

Establish a regression testing framework that validates all core engine features continue to work correctly after headless modifications, using frame capture and automated comparison.

## Acceptance Criteria

- [ ] Automated test harness: runs headless worker, captures frames, compares against baseline
- [ ] Reference screenshots generated for all base maps (start, e1m1–e1m8, e2m1–e2m7, e3m1–e3m7, e4m1–e4m8)
- [ ] SSIM comparison: each captured frame ≥ 0.85 similarity to reference
- [ ] Movement regression: scripted input → expected player position delta
- [ ] Sound regression: captured audio contains expected frequency content
- [ ] Console regression: command execution produces expected cvar changes
- [ ] Tests run in CI on every PR (Docker-based)
- [ ] Test run completes in < 10 minutes

## Test Cases

### Visual Regression Tests

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| VR-01 | Start map renders | Capture frame at t=2s | SSIM ≥ 0.85 vs baseline |
| VR-02 | e1m1 renders | Load e1m1, capture at t=2s | SSIM ≥ 0.85 vs baseline |
| VR-03 | HUD displays correctly | Capture frame, crop HUD region | SSIM ≥ 0.90 vs baseline |
| VR-04 | Lighting correct | Load map with varied lighting | SSIM ≥ 0.80 vs baseline |
| VR-05 | Monsters render | Load map with monsters, capture | Known entities visible |
| VR-06 | Particles render | Trigger explosion, capture | Frame diff from pre-explosion |
| VR-07 | Water/lava renders | View water surface | Animated texture present |

### Functional Regression Tests

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| FR-01 | Player moves forward | Inject W key for 1s, compare positions | Position delta > 100 units |
| FR-02 | Player looks around | Inject mouse dx=100, compare angles | Yaw changed by expected amount |
| FR-03 | Player jumps | Inject space, track Z position | Z position increases then returns |
| FR-04 | Console command | Inject "god" command | god mode cvar enabled |
| FR-05 | Map change | Inject "map e1m2" | New map loaded (frame differs) |
| FR-06 | Save/Load | Save game, respawn, load game | Position matches saved position |

### Audio Regression Tests

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| AR-01 | Sounds play | Load map with ambient sounds | Audio buffer non-zero |
| AR-02 | Weapon sound | Inject attack, capture audio | Frequency spike in expected range |
| AR-03 | Ambient sounds | Load map with water, capture 5s | Continuous audio content |

## Test Infrastructure

### Test Runner Script

```bash
#!/bin/bash
# regression-test.sh
set -e

IMAGE="quake-worker:test"
BASEDIR="/game"
RESULTS_DIR="./test-results"

# Build test image
docker build -t $IMAGE .

# Run each test
run_visual_test() {
    local map=$1
    local name=$2
    docker run --rm --cpus=2 -m 512m \
        -v $(pwd)/id1:$BASEDIR/id1:ro \
        -v $RESULTS_DIR:/results \
        -e QUAKE_BASEDIR=$BASEDIR \
        $IMAGE +map $map +wait 60 +captureframe /results/$name.rgba +quit
    
    # Compare with baseline
    python3 compare_frames.py baselines/$name.rgba $RESULTS_DIR/$name.rgba
}

run_visual_test "start" "start-map"
run_visual_test "e1m1" "e1m1"
# ... more tests

echo "All regression tests passed"
```

### Frame Comparison (Python)

```python
# compare_frames.py
import numpy as np
from skimage.metrics import structural_similarity as ssim
import sys

def load_rgba(path, width=1280, height=720):
    data = np.fromfile(path, dtype=np.uint8)
    return data.reshape((height, width, 4))[:, :, :3]  # Drop alpha

baseline = load_rgba(sys.argv[1])
captured = load_rgba(sys.argv[2])

score = ssim(baseline, captured, channel_axis=2)
print(f"SSIM: {score:.4f}")

if score < 0.85:
    print("FAIL: Visual regression detected")
    sys.exit(1)
print("PASS")
```

### CI Integration

```yaml
# .github/workflows/regression.yml
name: Regression Tests
on: [pull_request]
jobs:
  regression:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          lfs: true  # baselines stored in LFS
      - run: docker build -t quake-worker:test .
      - run: ./tests/regression-test.sh
      - uses: actions/upload-artifact@v4
        if: failure()
        with:
          name: test-results
          path: test-results/
```

## Baseline Generation

1. Build headless worker from known-good commit
2. Run each test scenario
3. Save captured frames as baselines in Git LFS
4. When intentional visual changes are made: regenerate baselines with PR review

## Risks

- Frame timing: asynchronous rendering may produce slightly different frames. Use `+wait` to stabilize before capture.
- Mesa LLVMpipe rendering may differ slightly from reference GL implementation. Accept lower SSIM threshold (0.85 instead of 0.95).
- Flaky tests: particle effects, dynamic lighting are non-deterministic. Exclude from strict comparison or use region masking.

## Rollback

Tests are read-only — no impact on production code. Remove test files to disable.
