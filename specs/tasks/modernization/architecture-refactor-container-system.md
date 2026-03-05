# Task: Architecture Refactor — Container System Layer

**Phase**: 1 (Headless Game Worker)  
**Priority**: P0  
**Estimated Effort**: 3–4 days  
**Prerequisites**: Headless video, audio, and input drivers

## Objective

Create `sys_container.c` that replaces platform-specific system layers with a container-ready system layer supporting SIGTERM handling, environment variable configuration, structured JSON logging, and HTTP health checks.

## Acceptance Criteria

- [ ] `sys_container.c` replaces `sys_linux.c` / `sys_win.c` for headless builds
- [ ] SIGTERM triggers graceful shutdown (game state saved if applicable, clean exit code 0)
- [ ] Configuration read from environment variables: `QUAKE_BASEDIR`, `QUAKE_MAP`, `QUAKE_SKILL`, `QUAKE_ARGS`
- [ ] All `Con_Printf` / `Sys_Printf` output as structured JSON to stdout
- [ ] HTTP health endpoint at `/healthz` returns 200 when game loop is running
- [ ] Container exits cleanly within 5 seconds of SIGTERM
- [ ] No references to platform-specific APIs (Win32, X11)

## Implementation Steps

### 1. sys_container.c Core

```c
// sys_container.c — Container-optimized system layer

#include "quakedef.h"
#include <signal.h>
#include <stdlib.h>
#include <time.h>

static volatile sig_atomic_t shutdown_requested = 0;

static void signal_handler(int sig)
{
    if (sig == SIGTERM || sig == SIGINT)
        shutdown_requested = 1;
}

void Sys_Init(void)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

void Sys_Quit(void)
{
    Host_Shutdown();
    exit(0);
}

void Sys_Error(char *error, ...)
{
    va_list argptr;
    char buf[1024];
    va_start(argptr, error);
    vsnprintf(buf, sizeof(buf), error, argptr);
    va_end(argptr);
    
    Sys_JsonLog("error", "fatal", buf);
    Host_Shutdown();
    exit(1);
}

// Check shutdown flag in main loop
qboolean Sys_ShutdownRequested(void)
{
    return shutdown_requested != 0;
}
```

### 2. Structured JSON Logging

```c
void Sys_JsonLog(const char *level, const char *component, const char *message)
{
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    
    // Escape message for JSON
    fprintf(stdout, "{\"timestamp\":\"%s\",\"level\":\"%s\","
            "\"component\":\"%s\",\"message\":\"%s\"}\n",
            timestamp, level, component, message);
    fflush(stdout);
}

// Replace Sys_Printf:
void Sys_Printf(char *fmt, ...)
{
    va_list argptr;
    char buf[1024];
    va_start(argptr, fmt);
    vsnprintf(buf, sizeof(buf), fmt, argptr);
    va_end(argptr);
    
    Sys_JsonLog("info", "engine", buf);
}
```

### 3. Environment Variable Configuration

```c
int main(int argc, char **argv)
{
    // Read config from environment
    const char *basedir = getenv("QUAKE_BASEDIR");
    const char *map = getenv("QUAKE_MAP");
    const char *skill = getenv("QUAKE_SKILL");
    const char *extra_args = getenv("QUAKE_ARGS");
    
    // Build argument list from env vars
    // Prepend basedir as -basedir, map as +map, etc.
    // Pass to Host_Init()
    
    Sys_Init();
    Host_Init(&parms);
    
    // Main loop
    while (!Sys_ShutdownRequested())
    {
        Host_Frame(/* frame time */);
    }
    
    Sys_Quit();
    return 0;
}
```

### 4. Health Check Endpoint

Minimal HTTP server on a separate thread (or use a lightweight approach):

```c
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

static volatile qboolean engine_healthy = false;
#define HEALTH_PORT 8080

static void *health_thread(void *arg)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    // ... bind to HEALTH_PORT, listen
    
    while (!shutdown_requested)
    {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) continue;
        
        const char *response = engine_healthy
            ? "HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nOK"
            : "HTTP/1.0 503 Service Unavailable\r\nContent-Length: 8\r\n\r\nstarting";
        
        send(client, response, strlen(response), 0);
        close(client);
    }
    close(server_fd);
    return NULL;
}

// Called from main after Host_Init() succeeds:
// engine_healthy = true;
```

### 5. Main Loop Integration

Add shutdown check to `Host_Frame()` or main loop:

```c
// In main loop:
while (!Sys_ShutdownRequested())
{
    Host_Frame(frame_time);
}
Sys_JsonLog("info", "engine", "Graceful shutdown initiated");
Sys_Quit();
```

## Files to Create

- `WinQuake/sys_container.c`

## Files to Modify

- `CMakeLists.txt` — select `sys_container.c` when `HEADLESS=ON`

## Validation

```bash
# Build
cmake -B build -DHEADLESS=ON -DNOASM=ON && cmake --build build

# Test startup with env vars
QUAKE_BASEDIR=/game QUAKE_MAP=e1m1 ./build/quake-worker &
PID=$!

# Test health check
sleep 5
curl -f http://localhost:8080/healthz  # Expect 200

# Test structured logging
docker logs <container> | head -5 | python3 -m json.tool  # Valid JSON

# Test graceful shutdown
kill -TERM $PID
wait $PID
echo $?  # Expect 0
```

## Risks

- Health check thread: Quake is single-threaded; adding a thread is a change. Keep health thread minimal — only socket accept/respond.
- JSON logging overhead: `fprintf` + `fflush` on every message. Acceptable at Quake's log volume.

## Rollback

Remove `sys_container.c`, revert CMakeLists.txt. Original `sys_linux.c` unchanged.
