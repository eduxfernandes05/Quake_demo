# Task: Security Remediation — Buffer Overflow Fixes

**Phase**: 0 (Foundation)  
**Priority**: P0  
**Estimated Effort**: 2–3 days  
**Prerequisites**: CMake build working

## Objective

Eliminate all buffer overflow vulnerabilities by replacing unsafe string functions with bounds-checked alternatives, and remove the `svc_stufftext` remote code execution vector.

## Acceptance Criteria

- [ ] Zero occurrences of `sprintf` in codebase (replaced with `snprintf`)
- [ ] Zero occurrences of unsafe `strcpy` where destination size is known (replaced with bounds-checked alternative)
- [ ] `svc_stufftext` command processing removed from `cl_parse.c`
- [ ] Path traversal prevention: file loading rejects paths containing `..`
- [ ] Build with AddressSanitizer passes 5-minute gameplay test with no violations
- [ ] `cppcheck --enable=all` reports no buffer overflow warnings
- [ ] All changes preserve existing functionality (visual output identical)

## Implementation Steps

### 1. Fix sprintf → snprintf

```bash
# Inventory all sprintf calls
grep -rn '\bsprintf\b' WinQuake/*.c | wc -l
```

For each occurrence, replace:
```c
// Before
sprintf(buf, "Loading %s", mapname);

// After
snprintf(buf, sizeof(buf), "Loading %s", mapname);
```

Special cases:
- When `buf` is a pointer (not array), determine buffer size from allocation
- When writing to global buffers, use the known buffer size constant
- `va()` function in `common.c`: ensure internal buffer is bounds-checked

### 2. Fix strcpy/strcat

Replace unbounded copies where buffer size is known:
```c
// Before
strcpy(dest, src);

// After
// Use snprintf for combining, or strlcpy-pattern:
snprintf(dest, sizeof(dest), "%s", src);
```

### 3. Remove svc_stufftext Processing

In `cl_parse.c`, find `svc_stufftext` case in `CL_ParseServerMessage()`:
```c
// Before: executes arbitrary commands from server
case svc_stufftext:
    s = MSG_ReadString();
    Cbuf_AddText(s);
    break;

// After: log and ignore
case svc_stufftext:
    s = MSG_ReadString();
    Con_Printf("Ignored svc_stufftext: server attempted command injection\n");
    break;
```

### 4. Path Traversal Prevention

In `common.c`, add path sanitization to file loading functions:
```c
// Add to COM_FindFile or COM_OpenFile
static qboolean COM_ValidatePath(const char *path)
{
    if (strstr(path, "..") != NULL)
        return false;
    if (path[0] == '/' || path[0] == '\\')
        return false;
    return true;
}
```

### 5. Validate with ASan

```bash
cmake -B build-asan -DNOASM=ON -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 ./build-asan/quake-worker +map e1m1 +wait 300 +quit
```

## Files to Modify

Key files with `sprintf` usage (non-exhaustive):
- `common.c`, `console.c`, `cmd.c`, `host.c`, `host_cmd.c`
- `cl_main.c`, `cl_parse.c`, `cl_demo.c`
- `sv_main.c`, `sv_user.c`
- `net_main.c`, `net_udp.c`
- `pr_cmds.c`, `pr_edict.c`
- `r_main.c`, `gl_rmain.c`
- `snd_dma.c`, `snd_mem.c`

## Risks

- `snprintf` may truncate long strings — verify no logic depends on full string
- Removing `svc_stufftext` breaks server-controlled config changes — acceptable for cloud (server and client are same container)

## Rollback

`git revert` — all changes are source-level, no infrastructure impact.
