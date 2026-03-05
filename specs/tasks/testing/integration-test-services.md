# Task: Integration Testing — Service-to-Service Communication

**Phase**: 2–3 (Streaming Gateway + Session Manager)  
**Priority**: P0  
**Estimated Effort**: 3–5 days  
**Prerequisites**: Game worker, streaming gateway, and session manager services containerized

## Objective

Validate that all services communicate correctly, container lifecycle management works, and the system handles edge cases (failures, timeouts, concurrent operations).

## Acceptance Criteria

- [ ] Docker Compose test environment spins up all 5 services locally
- [ ] Worker ↔ Gateway IPC: frames and audio flow, input is forwarded
- [ ] Session Manager → ACA (mocked): container provisioning lifecycle works
- [ ] Session Manager → Cosmos DB: session CRUD operations verified
- [ ] Telemetry API: events ingested from all services
- [ ] All services pass health checks within 10s of startup
- [ ] Tests run in < 3 minutes
- [ ] Tests run in CI on every PR

## Test Environment

### Docker Compose

```yaml
# docker-compose.test.yml
services:
  game-worker:
    build: ./WinQuake
    volumes:
      - ./id1:/game/id1:ro
      - shared-frames:/shared
    environment:
      QUAKE_BASEDIR: /game
      QUAKE_MAP: e1m1
    healthcheck:
      test: curl -f http://localhost:8080/healthz
      interval: 5s
      timeout: 3s
      retries: 5

  streaming-gateway:
    build: ./streaming-gateway
    volumes:
      - shared-frames:/shared
    depends_on:
      game-worker:
        condition: service_healthy
    ports:
      - "8443:8443"

  session-manager:
    build: ./session-manager
    environment:
      COSMOS_ENDPOINT: http://cosmos-emulator:8081
      ACA_MOCK: "true"
    depends_on:
      cosmos-emulator:
        condition: service_healthy
    ports:
      - "8080:8080"

  assets-api:
    build: ./assets-api
    volumes:
      - ./id1:/assets:ro
    ports:
      - "8081:8080"

  telemetry-api:
    build: ./telemetry-api
    ports:
      - "8082:8080"

  cosmos-emulator:
    image: mcr.microsoft.com/cosmosdb/linux/azure-cosmos-emulator:latest
    environment:
      AZURE_COSMOS_EMULATOR_PARTITION_COUNT: 1
    ports:
      - "8081:8081"

volumes:
  shared-frames:
```

## Test Cases

### Worker-Gateway Integration

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| WG-01 | Frame flow | Gateway reads from shared volume | Frames received > 0 in 5s |
| WG-02 | Audio flow | Gateway reads audio ring buffer | Non-zero PCM data |
| WG-03 | Input injection | Gateway sends key event → worker | Player position changes |
| WG-04 | Worker crash recovery | Kill worker → auto-restart | Gateway reconnects within 10s |
| WG-05 | Frame rate | Measure frame delivery rate | ≥ 28 fps over 10s |

### Session Manager Integration

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| SM-01 | Create session | POST /api/sessions | 201, session in Cosmos |
| SM-02 | Get session | GET /api/sessions/{id} | Session data returned |
| SM-03 | Delete session | DELETE /api/sessions/{id} | Session removed, cleanup triggered |
| SM-04 | List sessions | GET /api/sessions | Returns user's sessions |
| SM-05 | Duplicate create | Two POSTs with same params | Two distinct sessions |
| SM-06 | Delete nonexistent | DELETE /api/sessions/fake | 404 |
| SM-07 | Concurrent creates | 5 POSTs simultaneously | All 5 succeed |

### Assets API Integration

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| AA-01 | List maps | GET /api/assets/maps | JSON array includes "e1m1" |
| AA-02 | Health check | GET /api/assets/health | 200 |
| AA-03 | Missing asset | GET /api/assets/maps/nonexistent | 404 |

### Telemetry API Integration

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| TA-01 | Ingest event | POST /api/telemetry/events | 202 accepted |
| TA-02 | Invalid event | POST invalid JSON | 400 |
| TA-03 | Health check | GET /api/telemetry/health | 200 |

### Cross-Service Integration

| ID | Test | Method | Pass Condition |
| --- | --- | --- | --- |
| XS-01 | All healthy | Check all /healthz endpoints | All return 200 |
| XS-02 | Full flow | Create session → connect stream → capture frame → end | Success |
| XS-03 | Graceful shutdown | docker compose down | All containers exit 0 within 10s |

## Test Runner

```bash
#!/bin/bash
# integration-test.sh
set -e

echo "Starting test environment..."
docker compose -f docker-compose.test.yml up -d --build --wait

echo "Running integration tests..."
cd tests/integration
python -m pytest -v --timeout=120

echo "Collecting results..."
EXIT=$?

echo "Tearing down..."
docker compose -f docker-compose.test.yml down -v

exit $EXIT
```

## CI Configuration

```yaml
# .github/workflows/integration.yml
name: Integration Tests
on: [pull_request]
jobs:
  integration:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          lfs: true
      - run: ./tests/integration-test.sh
      - uses: actions/upload-artifact@v4
        if: failure()
        with:
          name: integration-logs
          path: test-results/
```

## Risks

- Cosmos emulator: slow to start (30-60s). Use health check with generous timeout.
- Shared volume timing: worker may not have frames ready when gateway starts. Gateway retries with backoff.
- Port conflicts: use Docker networking to avoid host port conflicts in CI.

## Rollback

Tests are read-only. Docker Compose `down -v` cleans up all resources.
