# Task: Dependency Upgrade — Build System and Compiler

**Phase**: 0 (Foundation)  
**Priority**: P0  
**Estimated Effort**: 3–5 days  
**Prerequisites**: Git repository initialized

## Objective

Replace the MSVC 6.0 build system with CMake targeting Linux/gcc, remove x86 assembly dependency, and establish a Docker multi-stage build.

## Acceptance Criteria

- [ ] `CMakeLists.txt` at project root builds all `.c` files for Linux (gcc ≥ 12)
- [ ] Build option `-DHEADLESS=ON` selects headless drivers (Phase 1 placeholder stubs)
- [ ] Build option `-DNOASM=ON` excludes all `.s` assembly files and uses C fallbacks
- [ ] `cmake -B build -DNOASM=ON && cmake --build build` exits 0 and produces binary
- [ ] Build with ASan: `-DCMAKE_C_FLAGS="-fsanitize=address"` produces binary and runs without violations for 10s
- [ ] Dockerfile multi-stage build produces container image < 500 MB
- [ ] `docker run quake-worker --help` or similar exits cleanly
- [ ] CI workflow (GitHub Actions) runs CMake build on push

## Implementation Steps

1. Create `CMakeLists.txt`:
   - `cmake_minimum_required(VERSION 3.25)`
   - Project name: `quake-worker`
   - Add all `.c` files from WinQuake/ as sources
   - Conditionally exclude platform-specific files based on options
   - When `-DNOASM=ON`: exclude all `.s` files, define `NOASM` preprocessor macro
   - Link: `-lm` (math), and conditionally Mesa GL libraries
   
2. Remove assembly dependency:
   - All assembly files (`.s`) excluded from build when `NOASM=ON`
   - Verify C fallback codepaths in `d_draw.c`, `r_draw.c`, etc. compile and link
   - Remove `asm_i386.h` includes when `NOASM` defined

3. Create `Dockerfile`:
   ```dockerfile
   FROM ubuntu:24.04 AS builder
   RUN apt-get update && apt-get install -y build-essential cmake
   COPY WinQuake/ /src/
   COPY CMakeLists.txt /src/
   WORKDIR /src
   RUN cmake -B build -DNOASM=ON && cmake --build build
   
   FROM ubuntu:24.04
   RUN useradd -r -s /usr/sbin/nologin quake
   COPY --from=builder /src/build/quake-worker /usr/local/bin/
   USER quake
   ENTRYPOINT ["quake-worker"]
   ```

4. Create `.github/workflows/ci.yml`:
   ```yaml
   name: CI
   on: [push, pull_request]
   jobs:
     build:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4
         - run: cmake -B build -DNOASM=ON
         - run: cmake --build build
   ```

## Risks

- Some C fallback paths may not compile without assembly — fix incrementally
- MSVC-specific code patterns (`__int64`, `_asm`) need `#ifdef` guards

## Rollback

Remove `CMakeLists.txt` and `Dockerfile` — original source files unchanged.

## Validation

```bash
cmake -B build -DNOASM=ON && cmake --build build
docker build -t quake-worker .
docker run --rm quake-worker --version  # or similar smoke test
```
