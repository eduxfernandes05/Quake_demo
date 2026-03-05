# Feature: Memory Management

## Feature ID
`memory-management`

## Purpose
Provide a custom memory management system that eliminates runtime allocation overhead, fragmentation, and the need for per-allocation freeing. The engine uses a fixed pre-allocated memory pool subdivided into four zones with different allocation semantics.

## Scope
- Hunk allocator: large permanent allocations (level data, models, progs)
- Zone allocator: small dynamic allocations (strings, temp objects)
- Cache allocator: purgeable allocations (textures, sounds, surface cache)
- Temp allocator: single-frame temporary allocations
- Memory debugging and statistics

## User Workflows

### Engine Startup
1. Engine allocates a single large memory block (default 16MB, configurable with `-mem <MB>`)
2. Hunk base established at bottom of block
3. Zone allocated from hunk (48KB default, configurable with `-zone <KB>`)
4. Cache and temp system initialized
5. All subsequent allocations come from this pre-allocated pool

### Level Loading
1. `Hunk_LowMark` records hunk position
2. BSP data loaded into hunk (vertices, planes, nodes, leaves, faces, etc.)
3. Models loaded into hunk
4. Sounds cached via Cache allocator
5. Surface cache allocated for software renderer
6. On level change: `Hunk_FreeToLowMark` reclaims all level-specific data

### Texture/Sound Caching
1. Asset requested (texture, sound, model skin)
2. `Cache_Check` tests if still in memory
3. If present: return pointer directly
4. If evicted: `Cache_Alloc` loads from disk, places in cache
5. Cache entries evicted (LRU) when memory pressure requires it

## Functional Requirements

### FR-MEM-01: Hunk Allocator
- Linear allocator from both ends of the memory block
- Low end: permanent data (engine structures, level data)
- High end: temporary loading data (freed after load completes)
- `Hunk_AllocName(size, name)`: Allocate from low end with 8-char debug name
- `Hunk_HighAllocName(size, name)`: Allocate from high end
- `Hunk_TempAlloc(size)`: Temporary allocation (freed next call)
- `Hunk_FreeToLowMark(mark)` / `Hunk_FreeToHighMark(mark)`: Stack-style free
- 16-byte alignment for all allocations
- Sentinel value `0x1df001ed` for corruption detection

### FR-MEM-02: Zone Allocator
- Traditional malloc/free within a fixed-size pool
- `Z_Malloc(size)`: Allocate (errors if exhausted)
- `Z_Free(ptr)`: Free block, merge adjacent free blocks
- `Z_Realloc(ptr, size)`: Resize allocation
- Free list maintained for block recycling
- Default zone size: 48KB (for small dynamic allocations)
- Used by: string copies, command buffers, small objects

### FR-MEM-03: Cache Allocator
- Purgeable allocation system for asset caching
- `Cache_Alloc(cache, size, name)`: Allocate (may evict others)
- `Cache_Check(cache)`: Test if allocation still valid
- `Cache_Free(cache)`: Explicitly free cached data
- Doubly-linked list of cache entries
- LRU eviction when cache space is needed
- Used by: sound data (`sfxcache_t`), model skins, surface cache overflow

### FR-MEM-04: Temp Allocation
- `Hunk_TempAlloc(size)`: Single-use temporary memory
- Automatically freed on next `Hunk_TempAlloc` call
- Used for: file loading buffers, temporary processing

### FR-MEM-05: Memory Debugging
- `hunk_sentinel` value for corruption detection
- `Hunk_Check()`: Validate all hunk sentinels
- Console commands:
  - `hunk_print` / `-hunkprint`: Dump hunk allocation list
  - Zone debug: block count, sizes, free list state
  - Cache: entry count, total cached bytes
- Memory statistics shown during level load

## Implementation Files

| File | Purpose |
|------|---------|
| `zone.c` | All allocators: Hunk, Zone, Cache, Temp (~800 lines) |
| `zone.h` | Memory system interface |
| `common.c` | `COM_Init` → `Memory_Init` call, file load buffers |
| `quakedef.h` | `MINIMUM_MEMORY` (0x550000 = ~5.5MB) |

## Configuration
| Parameter | Default | Description |
|-----------|---------|-------------|
| `-mem <MB>` | 16 | Total memory pool size |
| `-zone <KB>` | 48 | Zone allocator size |
| `-minmemory` | — | Use minimum memory (5.5MB) |
| `-heapsize <KB>` | 16384 | Alternate memory pool size |

## Dependencies
- **Host** (`host.c`): Memory initialization during `Host_Init`
- **Common** (`common.c`): File loading uses temp/hunk allocations
- **Every subsystem**: All subsystems allocate from Hunk/Zone/Cache

## Acceptance Criteria
- AC-01: Engine runs within configured memory pool without exceeding bounds
- AC-02: Level transitions properly reclaim hunk memory via marks
- AC-03: Cache eviction allows continuous play without memory exhaustion
- AC-04: Zone allocator handles dynamic alloc/free without fragmentation failure
- AC-05: Sentinel values detect memory corruption reliably
- AC-06: `-mem` parameter adjusts pool size correctly
- AC-07: Minimum memory mode (5.5MB) functions for basic gameplay
