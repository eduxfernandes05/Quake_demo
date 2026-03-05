# Task: Feature Validation — Streaming End-to-End

**Phase**: 2–3 (Streaming Gateway + Session Manager)  
**Priority**: P0  
**Estimated Effort**: 5–7 days  
**Prerequisites**: Streaming gateway + session manager deployed

## Objective

Validate the complete player experience from browser login through game streaming to session cleanup, ensuring all components work together correctly.

## Acceptance Criteria

- [ ] Automated E2E test suite using Playwright + WebRTC client
- [ ] Full session lifecycle tested: login → create session → play → end session
- [ ] Video stream validated: frames decoded, FPS ≥ 25
- [ ] Audio stream validated: audio packets received
- [ ] Input validated: key press → visual change in stream
- [ ] Multi-session: 5 concurrent sessions all streaming
- [ ] Tests run in staging environment via CI
- [ ] Test run completes in < 5 minutes

## Test Cases

### E2E Lifecycle Tests

| ID | Test | Steps | Pass Condition |
| --- | --- | --- | --- |
| E2E-01 | Full session lifecycle | Auth → create → stream → end | All steps succeed, cleanup complete |
| E2E-02 | Session creation | POST /api/sessions | 201, session ID returned |
| E2E-03 | Session becomes ready | Poll GET /api/sessions/{id} | Status "running" within 15s |
| E2E-04 | WebRTC connects | Connect to websocketUrl | ICE connected within 10s |
| E2E-05 | Video streams | Monitor WebRTC stats | framesDecoded > 0, fps ≥ 25 |
| E2E-06 | Audio streams | Monitor WebRTC stats | audioPacketsReceived > 0 |
| E2E-07 | Input works | Press W key via data channel | Frame change detected within 200ms |
| E2E-08 | Session cleanup | DELETE /api/sessions/{id} | 204, worker stopped within 30s |
| E2E-09 | Unauthenticated rejected | POST /api/sessions (no token) | 401 |
| E2E-10 | Session ownership | DELETE other user's session | 403 |

### Concurrent Session Tests

| ID | Test | Pass Condition |
| --- | --- | --- |
| CS-01 | 5 simultaneous sessions | All 5 streaming independently |
| CS-02 | Session isolation | Actions in session A not visible in session B |
| CS-03 | Staggered creation | Create 5 sessions over 30s, all become ready |
| CS-04 | Staggered cleanup | Delete 5 sessions, all workers cleaned within 60s |

### Resilience Tests

| ID | Test | Pass Condition |
| --- | --- | --- |
| RS-01 | Browser disconnects | WebRTC disconnects → session stays for 5 min then auto-cleans |
| RS-02 | Stream quality check | 60s stream: no freeze > 1s, no audio gap > 500ms |
| RS-03 | Reconnection | Disconnect WebRTC → reconnect → stream resumes |

## Test Framework

### Playwright + WebRTC Test Client

```typescript
// tests/e2e/session-lifecycle.spec.ts
import { test, expect } from '@playwright/test';

test('full session lifecycle', async ({ browser }) => {
  const context = await browser.newContext();
  const page = await context.newPage();
  
  // 1. Login (mock or real Entra ID)
  const token = await getTestToken();
  
  // 2. Create session
  const response = await page.request.post(`${API_URL}/api/sessions`, {
    headers: { 'Authorization': `Bearer ${token}` },
    data: { map: 'e1m1', skill: 1 }
  });
  expect(response.status()).toBe(201);
  const session = await response.json();
  
  // 3. Wait for ready
  await expect.poll(async () => {
    const status = await page.request.get(`${API_URL}/api/sessions/${session.id}`, {
      headers: { 'Authorization': `Bearer ${token}` }
    });
    return (await status.json()).status;
  }, { timeout: 15000 }).toBe('running');
  
  // 4. Connect WebRTC
  await page.goto(`${CLIENT_URL}?session=${session.id}&token=${token}`);
  
  // 5. Wait for video
  await page.waitForFunction(() => {
    const video = document.querySelector('video');
    return video && video.videoWidth > 0;
  }, { timeout: 10000 });
  
  // 6. Send input
  await page.keyboard.press('w');
  await page.waitForTimeout(500);
  
  // 7. Cleanup
  const del = await page.request.delete(`${API_URL}/api/sessions/${session.id}`, {
    headers: { 'Authorization': `Bearer ${token}` }
  });
  expect(del.status()).toBe(204);
});
```

### WebRTC Stats Validation

```typescript
async function validateStream(page: Page) {
  const stats = await page.evaluate(async () => {
    const pc = (window as any).__rtcPeerConnection;
    const stats = await pc.getStats();
    let framesDecoded = 0, audioPackets = 0;
    stats.forEach((report: any) => {
      if (report.type === 'inbound-rtp' && report.kind === 'video')
        framesDecoded = report.framesDecoded;
      if (report.type === 'inbound-rtp' && report.kind === 'audio')
        audioPackets = report.packetsReceived;
    });
    return { framesDecoded, audioPackets };
  });
  
  expect(stats.framesDecoded).toBeGreaterThan(0);
  expect(stats.audioPackets).toBeGreaterThan(0);
}
```

## CI Configuration

```yaml
# .github/workflows/e2e.yml
name: E2E Tests
on:
  push:
    branches: [main]
jobs:
  e2e:
    runs-on: ubuntu-latest
    environment: staging
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: '20' }
      - run: npx playwright install --with-deps chromium
      - run: npx playwright test tests/e2e/
        env:
          API_URL: ${{ vars.STAGING_API_URL }}
          CLIENT_URL: ${{ vars.STAGING_CLIENT_URL }}
          TEST_CLIENT_ID: ${{ secrets.TEST_CLIENT_ID }}
          TEST_CLIENT_SECRET: ${{ secrets.TEST_CLIENT_SECRET }}
```

## Risks

- WebRTC in headless Chromium: may need `--use-fake-ui-for-media-stream` flag
- Entra ID test accounts: create dedicated test user in tenant
- Network variability: staging environment latency may differ from production

## Rollback

Tests are read-only. No impact on production.
