# 🎮 Quake — Plano de Modernização & Decomposição em Microserviços

## 1. Visão Geral

O objetivo deste projeto é modernizar o código-fonte icónico do **Quake** (id Software, 1996), transformando a sua arquitetura monolítica em C numa plataforma baseada em **microserviços**, mantendo a jogabilidade original intacta.

### 1.1 Estado Atual da Codebase

| Componente | Localização | Linguagem | Descrição |
|---|---|---|---|
| **WinQuake** | `WinQuake/` | C + x86 ASM | Motor original (software rendering + OpenGL) |
| **QuakeWorld Client** | `QW/client/` | C | Cliente multiplayer com client-side prediction |
| **QuakeWorld Server** | `QW/server/` | C | Servidor dedicado multiplayer |
| **Game Logic** | `qw-qc/` | QuakeC | Lógica de jogo (armas, itens, física, triggers) |
| **Build System** | Makefiles, `.dsw` | — | Visual Studio 4.x, GNU Make (Linux/Solaris) |

### 1.2 Arquitetura Monolítica Atual

```
┌─────────────────────────────────────────────────────────┐
│                    QUAKE MONOLITH                        │
│                                                         │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌──────────┐  │
│  │ Rendering│ │  Sound   │ │ Networking│ │  Input   │  │
│  │ (SW/GL)  │ │  (DMA)   │ │ (UDP/IPX) │ │ (KB/M)   │  │
│  └────┬─────┘ └────┬─────┘ └─────┬─────┘ └────┬─────┘  │
│       │            │             │             │         │
│  ┌────▼────────────▼─────────────▼─────────────▼─────┐  │
│  │              Game Loop (host.c)                    │  │
│  │  ┌──────────────────────────────────────────────┐  │  │
│  │  │         QuakeC VM (pr_exec.c)                │  │  │
│  │  │  weapons.qc | items.qc | combat.qc | ...    │  │  │
│  │  └──────────────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────────────┘  │
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │ vid_win.c    │  │ snd_win.c    │  │ net_udp.c    │   │
│  │ vid_dos.c    │  │ snd_linux.c  │  │ net_ipx.c    │   │
│  │ gl_vidnt.c   │  │ snd_dos.c    │  │ net_loop.c   │   │
│  └──────────────┘  └──────────────┘  └──────────────┘   │
│       Platform Abstraction Layer (tightly coupled)       │
└─────────────────────────────────────────────────────────┘
```

---

## 2. Arquitetura-Alvo

### 2.1 Decisão Arquitetural: Minimonolito + Microserviços Periféricos

> **ADR-001:** O game server (Physics + Game Logic + Game State) mantém-se como um **minimonolito**
> por instância de jogo. Separar estes componentes em microserviços independentes introduziria
> latência inaceitável para um jogo real-time a 60+ fps (um frame = 16.6ms; gRPC hop = 0.5-2ms;
> Physics chama Game Logic centenas de vezes por frame). Este é o padrão da indústria (Valorant,
> Fortnite, Overwatch). Os serviços periféricos (Auth, Matchmaking, Assets, etc.) são
> microserviços verdadeiros pois não estão no critical path do game loop.

### 2.2 Diagrama da Arquitetura-Alvo

```
                         ┌──────────────────┐
                         │   API Gateway    │
                         │  (Envoy/gRPC)    │
                         └────────┬─────────┘
                                  │
         ┌────────────────────────┼─────────────────────────┐
         │                        │                         │
   ┌─────▼──────┐          ┌─────▼──────┐           ┌──────▼─────┐
   │  Auth &    │          │  Match-    │           │  Player    │
   │  Session   │          │  making    │           │  Profile   │
   │  Service   │          │  Service   │           │  Service   │
   │  (REST)    │          │  (gRPC)    │           │  (REST)    │
   └────────────┘          └─────┬──────┘           └────────────┘
                                 │
                        ┌────────▼─────────┐
                        │   Game Server    │
                        │   Orchestrator   │
                        │  (K8s operator)  │
                        └────────┬─────────┘
                                 │ spawns N pods
              ┌──────────────────┼──────────────────┐
              │                  │                  │
   ╔══════════▼══════╗ ╔════════▼════════╗ ╔═══════▼═════════╗
   ║  Game Server    ║ ║  Game Server    ║ ║  Game Server    ║
   ║  Instance #1    ║ ║  Instance #2    ║ ║  Instance #N    ║
   ║ ┌────────────┐  ║ ║                 ║ ║                 ║
   ║ │ Physics    │  ║ ║   (same arch)   ║ ║   (same arch)   ║
   ║ │ sv_phys    │  ║ ║                 ║ ║                 ║
   ║ ├────────────┤  ║ ╚═════════════════╝ ╚═════════════════╝
   ║ │ Game Logic │  ║
   ║ │ QuakeC VM  │  ║  ◄── Minimonolito: tudo no mesmo
   ║ ├────────────┤  ║      processo para zero-latency
   ║ │ Game State │  ║      entre Physics/Logic/State
   ║ │ Entities   │  ║
   ║ ├────────────┤  ║
   ║ │ Net Layer  │  ║
   ║ │ UDP+delta  │  ║
   ║ └────────────┘  ║
   ╚═════════════════╝
              │
              │ emits events (async)
              ▼
   ┌──────────────────┐
   │   Event Bus      │
   │  (NATS/Service   │
   │   Bus)           │
   └────────┬─────────┘
            │
   ┌────────┼──────────────────┐
   │        │                  │
   ▼        ▼                  ▼
┌────────┐ ┌────────────┐ ┌────────────┐
│ Asset  │ │ Telemetry  │ │ Leaderboard│
│ Service│ │ & Logging  │ │ & Stats    │
│ (CDN)  │ │ Service    │ │ Service    │
└────────┘ └────────────┘ └────────────┘

┌──────────────────────────────────────────────────────┐
│               CLIENTE (Modernizado)                   │
│  ┌───────────┐ ┌───────────┐ ┌───────────────────┐   │
│  │ Vulkan /  │ │ OpenAL    │ │ SDL2 Input +      │   │
│  │ GL 4.6    │ │ Soft      │ │ Window Mgmt       │   │
│  │ Renderer  │ │ 3D Audio  │ │                   │   │
│  └───────────┘ └───────────┘ └───────────────────┘   │
│  ┌───────────┐ ┌───────────┐ ┌───────────────────┐   │
│  │ ImGui     │ │ Net Layer │ │ Client-side       │   │
│  │ UI/HUD    │ │ gRPC+UDP  │ │ Prediction        │   │
│  └───────────┘ └───────────┘ └───────────────────┘   │
└──────────────────────────────────────────────────────┘
```

---

## 3. Fases do Projeto

### Fase 0 — Preparação & Fundação
> **Objetivo:** Tornar o código compilável em toolchains modernos e estabelecer CI/CD.

| # | Task | Ficheiros Impactados | Prioridade |
|---|------|---------------------|-----------|
| 0.1 | Criar build system moderno (CMake) para substituir Makefiles e `.dsw` | `CMakeLists.txt` (novo), `Makefile.*`, `qw.dsw` | 🔴 Alta |
| 0.2 | Corrigir warnings e erros em compiladores modernos (MSVC 2022, GCC 13+, Clang 17+) | `WinQuake/*.c`, `QW/**/*.c` | 🔴 Alta |
| 0.3 | Configurar CI/CD pipeline (GitHub Actions) | `.github/workflows/` (novo) | 🔴 Alta |
| 0.4 | Adicionar testes de smoke — o jogo compila e arranca | `tests/` (novo) | 🟡 Média |
| 0.5 | Documentar a arquitetura atual (diagramas C4) | `docs/architecture/` (novo) | 🟡 Média |
| 0.6 | Remover código legacy morto (DOS, IPX, serial, Gravis UltraSound) | `net_ipx.*`, `net_ser.*`, `snd_gus.c`, `snd_dos.c`, `vid_dos.c`, `dos_v2.c`, `in_dos.c` | 🟡 Média |

---

### Fase 1 — Abstração & Desacoplamento
> **Objetivo:** Introduzir camadas de abstração para separar responsabilidades antes de extrair serviços.
> **Pré-requisito:** Fase 0 concluída (código compila em toolchains modernos, CI green).

#### 1.1 — Eliminar Variáveis Globais Críticas
**Prioridade:** 🔴 Alta | **Branch:** `phase-1/eliminate-globals`

O Quake usa ~50 variáveis globais partilhadas entre todos os subsistemas. Esta é a barreira #1 para desacoplamento.

**Variáveis globais a encapsular:**

| Global | Ficheiro | Usado por | Ação |
|--------|----------|-----------|------|
| `host_frametime` | `host.c` | Tudo | Passar como parâmetro a cada subsistema |
| `cl` (client_state_t) | `cl_main.c` | Cliente inteiro | Encapsular em `client_context_t*` |
| `sv` (server_t) | `sv_main.c` | Servidor inteiro | Encapsular em `server_context_t*` |
| `cls` (client_static_t) | `cl_main.c` | Net, UI, demo | Encapsular em `client_context_t*` |
| `pr_global_struct` | `pr_edict.c` | QuakeC VM + servidor | Isolar dentro de `qcvm_context_t*` |
| `sv_player` | `sv_user.c` | Física, input | Passar como parâmetro |
| `r_refdef` | `r_main.c` | Renderer inteiro | Encapsular em `render_context_t*` |
| `key_dest` | `keys.c` | Input, menu, console | Encapsular em `input_context_t*` |

**Abordagem incremental:**
1. Criar structs de contexto (`server_context_t`, `client_context_t`, `render_context_t`)
2. Mover globals para dentro dessas structs
3. Passar ponteiro de contexto em vez de aceder globals — ficheiro a ficheiro
4. Validar que o jogo funciona identicamente após cada ficheiro migrado

**Critério de Done:** Zero globals partilhadas entre subsistemas. Cada subsistema recebe o seu contexto via ponteiro.

---

#### 1.2 — Platform Abstraction Layer (PAL)
**Prioridade:** 🔴 Alta | **Branch:** `phase-1/pal-interfaces`

Criar interfaces (headers com function pointers) para isolar o código de plataforma.

**Interfaces a criar:**

```c
// pal/pal_video.h
typedef struct {
    int  (*init)(int width, int height, int fullscreen);
    void (*shutdown)(void);
    void (*swap_buffers)(void);
    void (*set_mode)(int width, int height, int fullscreen);
    void*(*get_gl_proc)(const char* name);
} pal_video_t;

// pal/pal_audio.h
typedef struct {
    int  (*init)(int samplerate, int channels);
    void (*shutdown)(void);
    void (*submit_buffer)(const int16_t* samples, int count);
    void (*set_listener)(float pos[3], float fwd[3], float up[3]);
} pal_audio_t;

// pal/pal_input.h
typedef struct {
    void (*init)(void);
    void (*shutdown)(void);
    void (*poll_events)(void);  // preenche fila de eventos
    int  (*get_mouse_delta)(int* dx, int* dy);
} pal_input_t;

// pal/pal_network.h
typedef struct {
    int  (*init)(void);
    void (*shutdown)(void);
    int  (*open_socket)(int port);
    int  (*send)(int socket, const void* data, int len, const netadr_t* to);
    int  (*recv)(int socket, void* data, int maxlen, netadr_t* from);
} pal_network_t;
```

**Ficheiros impactados:**
| Plataforma atual | Ficheiros a abstrair | Implementação PAL |
|---|---|---|
| Windows Video | `vid_win.c`, `gl_vidnt.c` | `pal/sdl2/pal_video_sdl2.c` |
| Windows Audio | `snd_win.c` | `pal/sdl2/pal_audio_sdl2.c` |
| Windows Input | `in_win.c` | `pal/sdl2/pal_input_sdl2.c` |
| Linux Video | `vid_x.c`, `gl_vidlinux.c` | `pal/sdl2/pal_video_sdl2.c` (shared) |
| Linux Audio | `snd_linux.c` | `pal/sdl2/pal_audio_sdl2.c` (shared) |
| Networking | `net_udp.c`, `net_wins.c` | `pal/posix/pal_network_posix.c` |

**Critério de Done:** Todo o código de plataforma acedido exclusivamente via PAL. SDL2 como única implementação.

---

#### 1.3 — Migrar Rendering para SDL2 + OpenGL 3.3 Core
**Prioridade:** 🔴 Alta | **Branch:** `phase-1/sdl2-opengl33`

**Sub-tasks detalhadas:**

| # | Sub-task | Ficheiros | Detalhe |
|---|----------|-----------|---------|
| 1.3.1 | Substituir window creation por SDL2 | `gl_vidnt.c` → `pal_video_sdl2.c` | `SDL_CreateWindow` + `SDL_GL_CreateContext` |
| 1.3.2 | Carregar extensões GL via SDL2 ou GLAD | `gl_vidnt.c` (wglGetProcAddress) | Substituir por `gladLoadGLLoader(SDL_GL_GetProcAddress)` |
| 1.3.3 | Substituir `glBegin/glEnd` por VAO/VBO | `gl_rmain.c`, `gl_mesh.c`, `gl_rsurf.c` | Immediate mode → retained mode |
| 1.3.4 | Criar shaders GLSL para substituir fixed-function pipeline | Novo: `shaders/world.vert/.frag`, `shaders/alias.vert/.frag` | Replicar o look original via shaders |
| 1.3.5 | Migrar lightmaps para texture arrays | `gl_rsurf.c` | `glTexImage2D` → `glTexSubImage3D` com texture array |
| 1.3.6 | Remover software renderer | `r_*.c`, `d_*.c`, `d_*.s` (assembly) | ~30 ficheiros a remover/arquivar |
| 1.3.7 | Remover código GL 1.x deprecated | `gl_draw.c`, `gl_warp.c` | `glOrtho`, `glMatrixMode`, display lists |

**Referência:** O port Quakespasm prova que esta migração é viável. Usar como referência para os shaders GLSL.

**Critério de Done:** Jogo renderiza identicamente ao original usando GL 3.3 Core Profile + SDL2. Zero chamadas GL deprecated.

---

#### 1.4 — Migrar Áudio para OpenAL Soft
**Prioridade:** 🟡 Média | **Branch:** `phase-1/openal-audio`

| # | Sub-task | Detalhe |
|---|----------|---------|
| 1.4.1 | Substituir DMA buffer por OpenAL source/buffer model | `snd_dma.c` → `audio/audio_openal.c` |
| 1.4.2 | Implementar 3D spatial audio | Mapear `S_StartSound(origin, ...)` para `alSource3f(AL_POSITION, ...)` |
| 1.4.3 | Manter WAV loading existente | `snd_mem.c` — reutilizar, converter output para OpenAL buffers |
| 1.4.4 | Substituir CD audio por streaming | `cd_win.c` → OGG/Vorbis streaming via OpenAL |
| 1.4.5 | Remover drivers legacy | `snd_win.c`, `snd_linux.c`, `snd_gus.c`, `snd_dos.c`, `snd_sun.c` |

**API mapping:**

| Quake Original | OpenAL Equivalente |
|---|---|
| `S_Init()` | `alcOpenDevice()` + `alcCreateContext()` |
| `S_StartSound(origin, channel, sfx, vol, att)` | `alGenSources()` + `alSource3f(POSITION)` + `alSourcePlay()` |
| `S_Update(origin, forward, right, up)` | `alListener3f(POSITION)` + `alListenerfv(ORIENTATION)` |
| `S_StopAllSounds()` | `alSourceStopv()` para todos os sources |
| DMA mixing loop (`snd_mix.c`) | Eliminado — OpenAL faz mixing internamente |

**Critério de Done:** Todos os sons do jogo reproduzem corretamente com posicionamento 3D via OpenAL Soft.

---

#### 1.5 — Migrar Input para SDL2
**Prioridade:** 🟡 Média | **Branch:** `phase-1/sdl2-input`

| # | Sub-task | Detalhe |
|---|----------|---------|
| 1.5.1 | Substituir `GetAsyncKeyState`/DirectInput por SDL2 events | `in_win.c` → `pal_input_sdl2.c` |
| 1.5.2 | Mapear SDL2 keycodes para Quake key system | `keys.c` — criar tabela de mapeamento `SDL_SCANCODE_*` → `K_*` |
| 1.5.3 | Mouse capture com `SDL_SetRelativeMouseMode` | Substituir `SetCursorPos`/`ClipCursor` |
| 1.5.4 | Suporte a gamepad (novo) | SDL2 Game Controller API → mapear para `+forward`, `+attack`, etc. |

**Critério de Done:** Teclado, rato e (opcionalmente) gamepad funcionam via SDL2 em Windows, Linux e macOS.

---

#### 1.6 — Modernizar Networking (UDP puro)
**Prioridade:** 🟡 Média | **Branch:** `phase-1/modern-networking`

| # | Sub-task | Detalhe |
|---|----------|---------|
| 1.6.1 | Remover transports obsoletos | Apagar `net_ipx.c`, `net_wipx.c`, `net_ser.c`, `net_comx.c`, `net_bw.c`, `net_mp.c` |
| 1.6.2 | Abstrair sockets com PAL network | `net_udp.c`/`net_wins.c` → `pal_network_posix.c` (cross-platform) |
| 1.6.3 | Manter `net_chan.c` intacto | O reliable channel over UDP é sólido — não mexer |
| 1.6.4 | Adicionar IPv6 support | Estender `netadr_t` para suportar `AF_INET6` |
| 1.6.5 | Preparar interface para futuro gRPC (controlo) + UDP (game data) | Separar control plane (connect, auth) de data plane (game packets) |

**Critério de Done:** Networking funciona apenas com UDP moderno (IPv4+IPv6). Zero dependências de IPX/serial/modem.

---

#### 1.7 — Separar Game Loop: Cliente vs Servidor
**Prioridade:** 🔴 Alta | **Branch:** `phase-1/separate-game-loops`

Este é o passo mais crítico. Atualmente `Host_Frame()` em `host.c` contém TUDO:

```c
// host.c — ANTES (simplificado)
void Host_Frame(float time) {
    Sys_SendKeyEvents();     // input
    IN_Commands();           // input
    Cbuf_Execute();          // console commands
    CL_ReadPackets();        // client net
    SV_Frame();              // SERVER FRAME (physics, entities, QuakeC)
    CL_SetState();           // client state
    SCR_UpdateScreen();      // rendering
    S_Update();              // audio
    CL_SendCmd();            // client net send
    host_framecount++;
}
```

**Separação alvo:**

```c
// server/sv_loop.c — DEPOIS
void SV_Frame(server_context_t* ctx, float frametime) {
    SV_ReadPackets(ctx);           // ler input dos clientes
    SV_RunPhysics(ctx, frametime); // física + QuakeC
    SV_SendClientMessages(ctx);    // delta-compressed updates
}

// client/cl_loop.c — DEPOIS
void CL_Frame(client_context_t* ctx, float frametime) {
    CL_ReadPackets(ctx);           // ler server updates
    CL_Predict(ctx);               // client-side prediction
    CL_UpdateEntities(ctx);        // interpolação
    R_RenderView(ctx->render);     // rendering
    S_Update(ctx->audio);          // audio
    CL_SendCmd(ctx);               // enviar input ao server
}

// main.c — DEPOIS
int main() {
    server_context_t sv_ctx;
    client_context_t cl_ctx;
    while (running) {
        float dt = Sys_FrameTime();
        if (is_server) SV_Frame(&sv_ctx, dt);
        if (is_client) CL_Frame(&cl_ctx, dt);
    }
}
```

**Sub-tasks:**

| # | Sub-task | Detalhe |
|---|----------|---------|
| 1.7.1 | Extrair `SV_Frame()` como função standalone sem dependências de cliente | Remover referências a `cl.*`, `cls.*`, `scr_*` do código server |
| 1.7.2 | Extrair `CL_Frame()` como função standalone sem dependências de servidor | Remover referências a `sv.*`, `pr_*` do código client |
| 1.7.3 | Criar entry-points separados | `quake_server` (dedicated) e `quake_client` (com server embebido para single-player) |
| 1.7.4 | Manter modo listen-server | Cliente + servidor no mesmo processo para single-player e LAN |

**Critério de Done:** `quake_server` corre como binário standalone sem código de rendering/áudio. `quake_client` corre com server embebido ou liga-se a server remoto.

---

#### 1.8 — Isolar QuakeC VM como Biblioteca (`libqcvm`)
**Prioridade:** 🟡 Média | **Branch:** `phase-1/libqcvm`

**Ficheiros a extrair:**

| Ficheiro | Responsabilidade | Dependências externas a cortar |
|---|---|---|
| `pr_exec.c` (~1200 linhas) | Bytecode interpreter (VM loop) | `sv.*` globals, `Sys_Error` |
| `pr_edict.c` (~900 linhas) | Entity management (alloc, free, find) | `sv.edicts`, `sv.max_edicts` |
| `pr_cmds.c` (~1500 linhas) | Built-in functions (50+ builtins) | **Quase tudo** — este é o ficheiro mais acoplado |
| `pr_comp.h` | Bytecode format definitions | Nenhuma (já self-contained) |

**Estratégia para `pr_cmds.c`:**

O `pr_cmds.c` contém builtins como `PF_traceline`, `PF_sound`, `PF_spawn` que chamam diretamente código do servidor. Solução: **callback table**.

```c
// libqcvm/qcvm.h
typedef struct {
    void (*traceline)(float* v1, float* v2, int nomonsters, void* ent);
    void (*sound)(void* ent, int channel, const char* sample, float vol, float att);
    void* (*spawn)(void);
    void (*remove)(void* ent);
    // ... ~50 callbacks
} qcvm_callbacks_t;

typedef struct {
    dprograms_t*      progs;
    dfunction_t*      functions;
    dstatement_t*     statements;
    globalvars_t*     globals;
    edict_t*          edicts;
    int               num_edicts;
    int               max_edicts;
    qcvm_callbacks_t  callbacks;  // server implementa estes
} qcvm_t;

qcvm_t* QCVM_Init(const char* progs_dat, qcvm_callbacks_t* cbs);
void    QCVM_Execute(qcvm_t* vm, int fnum);
void    QCVM_Shutdown(qcvm_t* vm);
```

**Critério de Done:** `libqcvm` compila como biblioteca estática standalone. Server usa-a via callbacks. VM não tem `#include` de ficheiros do servidor.

---

#### 1.9 — Definir Interfaces entre Subsistemas
**Prioridade:** 🟡 Média | **Branch:** `phase-1/subsystem-interfaces`

**Headers de interface a criar:**

| Interface | API pública | Consumidores |
|---|---|---|
| `engine/i_renderer.h` | `R_Init`, `R_RenderView`, `R_DrawString`, `R_Shutdown` | Cliente |
| `engine/i_audio.h` | `S_Init`, `S_StartSound`, `S_Update`, `S_Shutdown` | Cliente |
| `engine/i_input.h` | `IN_Init`, `IN_PollEvents`, `IN_GetMouseDelta`, `IN_Shutdown` | Cliente |
| `engine/i_network.h` | `NET_Init`, `NET_SendPacket`, `NET_GetPacket`, `NET_Shutdown` | Client + Server |
| `engine/i_gameserver.h` | `SV_Init`, `SV_Frame`, `SV_Shutdown`, `SV_AddClient`, `SV_RemoveClient` | Orchestrator |
| `engine/i_qcvm.h` | `QCVM_Init`, `QCVM_Execute`, `QCVM_Shutdown` | Server |

**Regra:** Nenhum subsistema inclui headers de outro subsistema diretamente — apenas a interface.

**Critério de Done:** Compilar com `-Werror` garante que não há includes cruzados entre subsistemas.

---

### Fase 2 — Extração de Microserviços (Backend)
> **Objetivo:** Extrair serviços periféricos como microserviços independentes, mantendo o game server como minimonolito de alta performance.
> **Pré-requisito:** Fase 1 concluída (subsistemas desacoplados com interfaces claras).

#### Nota Arquitetural: Minimonolito vs Microserviços

> Physics, Game Logic (QuakeC VM) e Game State **permanecem no mesmo processo** dentro de cada
> instância de game server. A um frame rate de 60fps (16.6ms/frame), `sv_phys.c` chama `pr_exec.c`
> centenas de vezes por frame (uma vez por entidade com think function). Separar estes em serviços
> remotos adicionaria latência inaceitável (~0.5-2ms por RPC × centenas de calls = game unplayable).
> Este é o padrão da indústria para todos os jogos real-time multiplayer.

#### 2.1 — Game Server Minimonolito (Refactored)
**Prioridade:** 🔴 Alta | **Branch:** `phase-2/game-server-minimonolith`

Consolidar o game server como um binário deployável e auto-suficiente.

**Estrutura interna do game server:**

```
services/gameserver/
├── main.c                    # Entry point, config loading
├── sv_loop.c                 # Frame loop principal
├── sv_network.c              # Receber/enviar packets UDP
├── physics/
│   ├── sv_phys.c             # Simulação de física
│   └── sv_move.c             # Pathfinding, movement
├── gamelogic/
│   ├── libqcvm/              # QuakeC VM (da Fase 1.8)
│   └── sv_progs.c            # Interface VM ↔ server
├── state/
│   ├── sv_ents.c             # Entity state management
│   ├── sv_world.c            # BSP world, collision hulls
│   └── sv_delta.c            # Delta compression para clients
├── clients/
│   ├── sv_user.c             # Client input processing
│   └── sv_send.c             # Broadcast state to clients
├── api/
│   ├── sv_api_grpc.c         # gRPC endpoints (admin, health)
│   └── sv_events.c           # Emitir eventos para Event Bus
└── proto/
    └── gameserver.proto       # Protobuf definitions
```

**API gRPC do Game Server (controlo, NÃO gameplay):**

```protobuf
service GameServerControl {
    rpc GetStatus(Empty) returns (ServerStatus);          // health check
    rpc GetPlayerList(Empty) returns (PlayerList);         // jogadores ligados
    rpc KickPlayer(KickRequest) returns (KickResponse);   // admin
    rpc ChangeMap(MapRequest) returns (MapResponse);       // mudar mapa
    rpc Shutdown(Empty) returns (Empty);                   // graceful shutdown
}
```

**Eventos emitidos para Event Bus (assíncronos):**

| Evento | Quando | Consumidores |
|--------|--------|-------------|
| `player.connected` | Jogador entra no servidor | Telemetry, Leaderboard |
| `player.disconnected` | Jogador sai | Telemetry, Leaderboard |
| `player.killed` | Frag registado | Leaderboard, Stats |
| `match.started` | Mapa carregado, jogo inicia | Telemetry, Matchmaking |
| `match.ended` | Timelimit/fraglimit atingido | Leaderboard, Matchmaking |
| `server.health` | A cada 10s | Orchestrator, Telemetry |

**Critério de Done:** Game server corre como binário standalone, aceita conexões UDP de clientes, expõe gRPC para controlo, emite eventos para event bus.

---

#### 2.2 — Auth & Session Service
**Prioridade:** 🔴 Alta | **Branch:** `phase-2/auth-service` | **Linguagem:** Rust (ou Go)

Atualmente a "autenticação" é apenas o packet `clc_connect` com um nome. Vamos substituir por auth real.

**Responsabilidades:**
- Registo e login de jogadores (email/password, OAuth com Discord/Steam)
- Emissão de JWT tokens com claims (player_id, display_name, roles)
- Validação de tokens (chamado pelo Game Server antes de aceitar conexão)
- Gestão de sessões ativas (tracking de quem está online)
- Rate-limiting de tentativas de login

**API (REST + gRPC):**

```protobuf
service AuthService {
    rpc Register(RegisterRequest) returns (AuthResponse);
    rpc Login(LoginRequest) returns (AuthResponse);        // retorna JWT
    rpc ValidateToken(TokenRequest) returns (TokenClaims);  // game server chama isto
    rpc RefreshToken(RefreshRequest) returns (AuthResponse);
    rpc Logout(LogoutRequest) returns (Empty);
}
```

**Data Store:** PostgreSQL (users table) + Redis (sessions, rate-limit counters)

**Schema:**

```sql
CREATE TABLE players (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username    VARCHAR(32) UNIQUE NOT NULL,
    email       VARCHAR(255) UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at  TIMESTAMPTZ DEFAULT now(),
    last_login  TIMESTAMPTZ
);

CREATE TABLE sessions (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    player_id   UUID REFERENCES players(id),
    token_hash  TEXT NOT NULL,
    created_at  TIMESTAMPTZ DEFAULT now(),
    expires_at  TIMESTAMPTZ NOT NULL
);
```

**Fluxo de conexão ao game server (com auth):**

```
Cliente                Auth Service             Game Server
  │                        │                        │
  ├── POST /login ────────►│                        │
  │◄── JWT token ──────────┤                        │
  │                        │                        │
  ├── UDP connect + JWT ───┼───────────────────────►│
  │                        │◄── ValidateToken(JWT) ─┤
  │                        ├── TokenClaims ────────►│
  │                        │                        ├── aceita conexão
  │◄── accept ─────────────┼────────────────────────┤
```

**Critério de Done:** Jogadores autenticam-se via Auth Service. Game Server valida JWT antes de aceitar conexão. Tentativas sem token válido são rejeitadas.

---

#### 2.3 — Matchmaking Service
**Prioridade:** 🟡 Média | **Branch:** `phase-2/matchmaking-service` | **Linguagem:** Go (ou Rust)

Substitui o master server query protocol original (`net_main.c`).

**Responsabilidades:**
- Manter registo de game servers ativos (heartbeats)
- Server browser: listar servidores com filtros (mapa, jogadores, ping, região)
- Matchmaking queue: juntar jogadores em lobbies por skill/região
- Pedir ao Orchestrator para spawner novos game servers se necessário

**API:**

```protobuf
service MatchmakingService {
    rpc ListServers(ServerFilter) returns (ServerList);
    rpc JoinQueue(JoinRequest) returns (stream MatchStatus);  // streaming até match found
    rpc RegisterServer(ServerInfo) returns (RegisterResponse); // game server regista-se
    rpc Heartbeat(ServerHealth) returns (Empty);               // game server heartbeat
    rpc DeregisterServer(ServerId) returns (Empty);
}
```

**Data Store:** Redis (server list com TTL, matchmaking queues)

**Critério de Done:** Clientes encontram servidores via Matchmaking Service. Servidores registam-se automaticamente ao iniciar.

---

#### 2.4 — Asset Service
**Prioridade:** 🟡 Média | **Branch:** `phase-2/asset-service` | **Linguagem:** Go + CDN

**Responsabilidades:**
- Servir ficheiros de assets: maps (`.bsp`), modelos (`.mdl`), sons (`.wav`), texturas
- Versionamento de assets (hash-based)
- Download on-demand pelo cliente (em vez de exigir todos os ficheiros locais)
- Cache layer (CDN / Azure Blob Storage)

**API (REST):**

```
GET /assets/{type}/{name}              # download asset
GET /assets/{type}/{name}/metadata     # size, hash, version
GET /assets/manifest/{map_name}        # lista de assets necessários para um mapa
HEAD /assets/{type}/{name}             # check if exists + hash
```

**Fluxo de loading de mapa:**

```
Cliente                    Asset Service              CDN/Blob Storage
  │                            │                           │
  ├── GET /manifest/e1m1 ─────►│                           │
  │◄── [bsp, 5 mdl, 20 wav] ──┤                           │
  │                            │                           │
  │ (para cada asset em falta)                             │
  ├── GET /assets/maps/e1m1 ──►│──── cache miss ──────────►│
  │◄── binary data ────────────┤◄── binary data ───────────┤
  │                            │    (cached para próximos)  │
```

**Critério de Done:** Cliente descarrega assets via HTTP em vez de exigir PAK files locais. Cache eficiente para downloads repetidos.

---

#### 2.5 — Leaderboard & Stats Service
**Prioridade:** 🟢 Baixa | **Branch:** `phase-2/leaderboard-service` | **Linguagem:** Rust (ou Go)

**Responsabilidades:**
- Receber eventos de kills/deaths do Event Bus
- Calcular e manter rankings (Elo, K/D ratio, accuracy)
- Histórico de partidas por jogador
- Leaderboards globais e por mapa

**Eventos consumidos:**

| Evento | Dados | Ação |
|--------|-------|------|
| `player.killed` | killer_id, victim_id, weapon, map | Update K/D, weapon stats |
| `match.ended` | map, duration, scores[], mvp_id | Update Elo, match history |

**API:**

```protobuf
service LeaderboardService {
    rpc GetPlayerStats(PlayerId) returns (PlayerStats);
    rpc GetLeaderboard(LeaderboardRequest) returns (LeaderboardResponse);
    rpc GetMatchHistory(PlayerId) returns (MatchHistoryResponse);
}
```

**Data Store:** PostgreSQL (stats, match history) + Redis (cached leaderboards)

**Critério de Done:** Stats atualizam automaticamente via eventos. Leaderboard acessível via API.

---

#### 2.6 — Telemetry & Logging Service
**Prioridade:** 🟢 Baixa | **Branch:** `phase-2/telemetry-service` | **Linguagem:** Go

**Responsabilidades:**
- Agregar logs estruturados de todos os serviços
- Métricas de performance (fps do server, tick rate, packet loss, player count)
- Alerting (server down, high latency, player surge)
- Dashboards (Grafana)

**Eventos consumidos:** Todos os eventos do Event Bus + logs via OpenTelemetry.

**Stack:**
- **Logs:** OpenTelemetry Collector → Azure Monitor / Loki
- **Métricas:** Prometheus (scrape de endpoints `/metrics` de cada serviço)
- **Traces:** OpenTelemetry → Jaeger / Azure Application Insights
- **Dashboards:** Grafana

**Critério de Done:** Dashboard mostra em real-time: servidores ativos, jogadores online, latência média, erros. Alertas disparam quando servidor fica unresponsive.

---

#### 2.7 — Game Server Orchestrator
**Prioridade:** 🟡 Média | **Branch:** `phase-2/orchestrator` | **Linguagem:** Go + Kubernetes API

**Responsabilidades:**
- Spawner/destruir pods de game server em Kubernetes
- Pool de servidores warm (pré-alocados para reduzir startup time)
- Responder a pedidos do Matchmaking Service ("preciso de um servidor na região EU")
- Health monitoring de game servers (restart de pods unhealthy)
- Graceful shutdown (esperar match acabar antes de matar pod)

**Implementação:** Kubernetes Operator (Custom Resource Definition) ou simples controller com K8s API.

```yaml
# CRD: GameServer
apiVersion: quake.modernized/v1
kind: GameServer
metadata:
  name: gameserver-eu-001
spec:
  map: "e1m1"
  maxPlayers: 16
  region: "eu-west"
  image: "quake-gameserver:v2.1"
status:
  phase: Running
  players: 12
  uptime: "2h34m"
```

**Critério de Done:** Orchestrator cria/destrói game servers dinamicamente. Matchmaking pede servidores via gRPC. Servidores inativos são reciclados.

---

### Fase 3 — Modernização do Cliente
> **Objetivo:** Modernizar o cliente mantendo a gameplay original fiel ao Quake de 1996.
> **Pré-requisito:** Fase 1 concluída (PAL + SDL2). Fase 2 parcialmente concluída (Auth + Game Server funcional).

#### 3.1 — Renderer Vulkan / OpenGL 4.6
**Prioridade:** 🔴 Alta | **Branch:** `phase-3/vulkan-renderer`

**Abordagem dual-backend:**

| Backend | Target | Quando usar |
|---------|--------|-------------|
| OpenGL 4.6 | Windows, Linux, macOS (via MoltenVK) | Default para compatibilidade máxima |
| Vulkan 1.3 | Windows, Linux | Para hardware moderno, melhor performance |

**Sub-tasks:**

| # | Sub-task | Detalhe |
|---|----------|---------|
| 3.1.1 | Abstrair renderer com interface `i_renderer.h` | Permitir swap entre GL e Vulkan |
| 3.1.2 | Implementar BSP rendering (world geometry) | Frustum culling, PVS (Potentially Visible Set), lightmaps |
| 3.1.3 | Implementar alias model rendering (.mdl) | Vertex animation interpolation (como Quake original) |
| 3.1.4 | Implementar particle system | Point sprites ou instanced quads |
| 3.1.5 | Implementar dynamic lighting | Per-pixel lighting nos shaders |
| 3.1.6 | Modo "Retro" | Filtro CRT, paleta original de 256 cores, pixelation shader |
| 3.1.7 | Suporte a resoluções modernas | 4K, ultrawide (21:9), 120Hz+, VSync options |
| 3.1.8 | HDR rendering (opcional) | Tone mapping, bloom |

**Critério de Done:** Jogo renderiza com visual equivalente ao original. Modo retro disponível. 60+ fps em hardware moderno a 4K.

---

#### 3.2 — Sistema de UI (ImGui)
**Prioridade:** 🟡 Média | **Branch:** `phase-3/imgui-ui`

| # | Sub-task | Detalhe |
|---|----------|---------|
| 3.2.1 | Integrar Dear ImGui com SDL2 + GL/Vulkan backend | Drop-in, ~100 linhas de integração |
| 3.2.2 | Recriar Main Menu | Start, Multiplayer, Options, Quit — com visual modernizado |
| 3.2.3 | Recriar HUD/Status Bar | Health, armor, ammo, face — respeitando layout original |
| 3.2.4 | Server Browser UI | Lista de servidores do Matchmaking Service, ping, player count |
| 3.2.5 | Settings menu moderno | Resolution, graphics quality, keybinds, audio volume |
| 3.2.6 | Scoreboard overlay | Tab para ver scores durante o jogo |
| 3.2.7 | Developer console | Manter consola estilo Quake (`~` para toggle) |

**Critério de Done:** Todos os menus funcionais e navegáveis com rato e teclado. Console developer funcional.

---

#### 3.3 — Áudio Espacial 3D (OpenAL Soft)
**Prioridade:** 🟡 Média | **Branch:** `phase-3/spatial-audio`

Continuação da Fase 1.4 com features avançadas:

| # | Feature | Detalhe |
|---|---------|---------|
| 3.3.1 | HRTF (Head-Related Transfer Function) | OpenAL Soft suporta nativamente — ativar para headphones |
| 3.3.2 | Reverb por ambiente | Aplicar EFX reverb diferente em caves vs outdoor |
| 3.3.3 | Oclusão de som | Sons atrás de paredes atenuados (line trace no BSP) |
| 3.3.4 | Música dinâmica | Streaming OGG/Vorbis em vez de CD audio |

**Critério de Done:** Sons posicionados em 3D com HRTF. Reverb varia por ambiente. Música streamed.

---

#### 3.4 — Protocolo de Rede Moderno (Cliente)
**Prioridade:** 🔴 Alta | **Branch:** `phase-3/modern-net-protocol`

O cliente precisa de comunicar com dois planos diferentes:

| Plano | Protocolo | Usado para |
|-------|-----------|-----------|
| **Control Plane** | gRPC/REST (TCP) | Auth, matchmaking, server browser, asset download |
| **Data Plane** | UDP (protocolo Quake melhorado) | Gameplay: input commands, entity state updates |

**Sub-tasks:**

| # | Sub-task | Detalhe |
|---|----------|---------|
| 3.4.1 | Integrar HTTP client (libcurl ou similar) | Comunicar com Auth, Asset, Matchmaking services |
| 3.4.2 | Implementar login flow no cliente | Tela de login → Auth Service → JWT → armazenar token |
| 3.4.3 | Manter protocolo UDP para gameplay | `net_chan.c` — mantém-se, é eficiente |
| 3.4.4 | Enviar JWT no connection handshake UDP | Game server valida token antes de aceitar |
| 3.4.5 | Implementar reconnection logic | Se desconectar, tentar reconectar com mesmo token |

**Critério de Done:** Cliente usa REST/gRPC para serviços periféricos e UDP para gameplay. Auth integrado no fluxo de conexão.

---

### Fase 4 — Infraestrutura & DevOps
> **Objetivo:** Cloud-native deployment com observabilidade completa.
> **Pré-requisito:** Pelo menos 2-3 serviços da Fase 2 funcionais.

#### 4.1 — Containerização
**Prioridade:** 🔴 Alta | **Branch:** `phase-4/dockerize`

**Dockerfile por serviço:**

| Serviço | Base Image | Tamanho estimado | Notas |
|---------|-----------|-----------------|-------|
| Game Server | `debian:bookworm-slim` | ~50MB | C binary + assets mínimos |
| Auth Service | `rust:slim` / `golang:alpine` | ~30MB | Multi-stage build |
| Matchmaking | `golang:alpine` | ~20MB | Lightweight |
| Asset Service | `golang:alpine` + nginx | ~25MB | Static file serving |
| Leaderboard | `rust:slim` | ~30MB | - |
| Telemetry | `golang:alpine` | ~20MB | + OTel collector sidecar |
| Orchestrator | `golang:alpine` | ~20MB | Precisa K8s RBAC |

**Docker Compose para desenvolvimento local:**

```yaml
services:
  gameserver:
    build: ./services/gameserver
    ports: ["26000:26000/udp", "50051:50051"]  # game + gRPC
    depends_on: [auth, nats]
  auth:
    build: ./services/auth
    ports: ["8080:8080"]
    depends_on: [postgres, redis]
  matchmaking:
    build: ./services/matchmaking
    ports: ["8081:8081"]
    depends_on: [redis]
  asset-service:
    build: ./services/assets
    ports: ["8082:8082"]
    volumes: ["./game-assets:/assets:ro"]
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_DB: quake
      POSTGRES_USER: quake
      POSTGRES_PASSWORD_FILE: /run/secrets/db_password
  redis:
    image: redis:7-alpine
  nats:
    image: nats:2.10-alpine
    ports: ["4222:4222"]
```

**Critério de Done:** `docker compose up` inicia todos os serviços. Jogo funcional em ambiente local containerizado.

---

#### 4.2 — Kubernetes (AKS)
**Prioridade:** 🔴 Alta | **Branch:** `phase-4/kubernetes`

**Estrutura de deployment:**

```
k8s/
├── base/
│   ├── namespace.yaml
│   ├── network-policies.yaml
│   └── resource-quotas.yaml
├── services/
│   ├── auth/
│   │   ├── deployment.yaml
│   │   ├── service.yaml
│   │   ├── hpa.yaml
│   │   └── configmap.yaml
│   ├── matchmaking/
│   ├── assets/
│   ├── leaderboard/
│   └── telemetry/
├── gameservers/
│   ├── gameserver-crd.yaml       # Custom Resource Definition
│   ├── gameserver-operator.yaml  # Orchestrator deployment
│   └── gameserver-template.yaml  # Pod template para game servers
├── infrastructure/
│   ├── postgres-statefulset.yaml
│   ├── redis-deployment.yaml
│   ├── nats-deployment.yaml
│   └── envoy-gateway.yaml
└── monitoring/
    ├── prometheus.yaml
    ├── grafana.yaml
    └── otel-collector.yaml
```

**Decisões de deploy:**

| Serviço | Replicas | Scaling | Notas |
|---------|----------|---------|-------|
| Auth | 2-5 | HPA (CPU/RPS) | Stateless, fácil de escalar |
| Matchmaking | 2-3 | HPA (queue depth) | Stateless |
| Asset Service | 2 + CDN | HPA (RPS) | CDN faz o heavy lifting |
| Leaderboard | 2 | HPA (event backlog) | Tolera atraso (eventual consistency) |
| Game Server | N | Orchestrator (custom) | 1 pod = 1 match. `hostNetwork: true` para low latency |
| Orchestrator | 1 (leader election) | - | Singleton com failover |

**Critério de Done:** Deploy automatizado via `kubectl apply` ou Helm. Game servers escaláveis. Serviços com health checks e readiness probes.

---

#### 4.3 — API Gateway (Envoy)
**Prioridade:** 🔴 Alta | **Branch:** `phase-4/api-gateway`

| Feature | Implementação |
|---------|--------------|
| Routing | `/api/auth/*` → Auth Service, `/api/match/*` → Matchmaking, etc. |
| Auth | JWT validation no gateway (não precisa de chamar Auth Service para cada request) |
| Rate limiting | 100 req/min por IP para auth, 1000 req/min para API geral |
| TLS termination | Let's Encrypt / Azure managed certificates |
| gRPC transcoding | REST ↔ gRPC automático para serviços protobuf |
| CORS | Permitir web client (se houver) |

---

#### 4.4 — Event Bus (NATS)
**Prioridade:** 🟡 Média | **Branch:** `phase-4/event-bus`

**Escolha: NATS** (alternativa: Azure Service Bus para produção)

| Topic | Publisher | Subscribers |
|-------|-----------|-------------|
| `game.player.connected` | Game Server | Telemetry, Matchmaking |
| `game.player.disconnected` | Game Server | Telemetry, Matchmaking, Leaderboard |
| `game.player.killed` | Game Server | Leaderboard, Telemetry |
| `game.match.started` | Game Server | Telemetry, Matchmaking |
| `game.match.ended` | Game Server | Leaderboard, Telemetry, Matchmaking |
| `server.health` | Game Server | Orchestrator, Telemetry |
| `server.requested` | Matchmaking | Orchestrator |
| `server.ready` | Orchestrator | Matchmaking |

**Critério de Done:** Todos os serviços publicam/consumem eventos via NATS. Mensagens persistidas com JetStream para replay.

---

#### 4.5 — Observabilidade
**Prioridade:** 🟡 Média | **Branch:** `phase-4/observability`

**Stack:**

```
Serviços ──► OpenTelemetry Collector ──┬──► Prometheus (métricas)
                                       ├──► Loki (logs)
                                       └──► Jaeger (traces)
                                              │
                                        Grafana (dashboards)
```

**Dashboards essenciais:**

| Dashboard | Métricas |
|-----------|---------|
| **Game Servers Overview** | Servidores ativos, jogadores total, tick rate médio, packet loss |
| **Player Activity** | Jogadores online, matchmaking queue size, avg session duration |
| **Service Health** | Request latency (p50/p95/p99), error rate, pod restarts |
| **Infrastructure** | CPU/RAM por serviço, network I/O, disk usage (PostgreSQL) |

**Alertas:**

| Alerta | Condição | Ação |
|--------|----------|------|
| 🔴 Game server down | Pod unhealthy > 30s | PagerDuty + auto-restart |
| 🟡 High latency | p99 > 100ms no Auth | Slack notification |
| 🟡 Matchmaking queue stuck | Queue > 50 players > 2min | Scale up game servers |
| 🔴 Database connection pool exhausted | Active connections > 90% | Alert + investigate |

**Critério de Done:** Dashboards operacionais em Grafana. Alertas configurados e testados.

---

### Fase 5 — Novas Funcionalidades
> **Objetivo:** Features que só são possíveis com a nova arquitetura.
> **Pré-requisito:** Fases 0-4 concluídas. Sistema estável em produção.

#### 5.1 — Player Profiles & Progression
**Prioridade:** 🟡 Média | **Branch:** `phase-5/player-profiles`

- Perfil persistente com stats (total kills, deaths, favorite weapon, maps played)
- Avatar e display name customizáveis
- Integração com Leaderboard Service para mostrar rank
- API REST para web profile page

#### 5.2 — Skill-Based Matchmaking (SBMM)
**Prioridade:** 🟡 Média | **Branch:** `phase-5/sbmm`

- Sistema Elo/Glicko-2 para rating de skill
- Matchmaking por faixas de skill (±200 Elo)
- Ajuste de rating após cada match
- Placement matches para novos jogadores (10 matches)
- Proteção contra smurf accounts (correlação de métricas comportamentais)

#### 5.3 — Replay System
**Prioridade:** 🟢 Baixa | **Branch:** `phase-5/replays`

- Game server grava todos os inputs + entity states por frame (event sourcing)
- Replays armazenados no Asset Service
- Cliente reproduz replay recriando game state frame a frame
- Free camera no replay mode
- Exportar highlights como vídeo (server-side rendering)

#### 5.4 — Mod Support via API
**Prioridade:** 🟢 Baixa | **Branch:** `phase-5/mod-api`

- API para carregar QuakeC mods customizados (`.dat` files)
- Sandbox: mods correm na QuakeC VM com permissões limitadas
- Mod marketplace: upload, download, rating de mods
- Server anuncia mod ativo → cliente descarrega automaticamente via Asset Service

#### 5.5 — Enhanced Spectator Mode
**Prioridade:** 🟢 Baixa | **Branch:** `phase-5/spectator`

- Spectator segue jogadores com câmara automática (overhead, first-person, chase)
- Minimap overlay com posições de jogadores
- Live streaming integration (RTMP output para Twitch/YouTube)
- Delay de 30s para anti-cheat em torneios

#### 5.6 — Anti-Cheat Server-Side
**Prioridade:** 🟡 Média | **Branch:** `phase-5/anticheat`

- Validação server-side de movement (speed hack detection)
- Análise estatística de aim (aimbot detection via accuracy anomalies)
- Rate-of-fire validation contra weapon mods
- Report system: jogadores reportam → review manual ou ML-assisted
- Bans centralizados no Auth Service (ban por account, não por IP)

---

## 4. Stack Tecnológica Proposta

| Camada | Tecnologia | Justificação |
|--------|-----------|-------------|
| **Game Server** | C (mantém codebase original, refactored) | Zero overhead, máxima performance |
| **Serviços periféricos** | Rust / Go | Rust para performance-critical (Leaderboard), Go para I/O-bound (Matchmaking, Orchestrator) |
| **Cliente** | C++ 20 | Compatibilidade com Vulkan/GL, SDL2 |
| **Comunicação síncrona** | gRPC (Protocol Buffers) | Tipagem forte, code-gen, baixa latência |
| **Comunicação assíncrona** | NATS JetStream | Lightweight, persistência de mensagens, at-least-once delivery |
| **Gameplay networking** | UDP com net_chan (protocolo Quake melhorado) | Latência mínima para game data |
| **Rendering** | Vulkan 1.3 + OpenGL 4.6 (dual backend) | Vulkan para performance, GL para compatibilidade |
| **Áudio** | OpenAL Soft | 3D spatial, HRTF, cross-platform |
| **Window/Input** | SDL2 | Cross-platform, maduro, game-friendly |
| **Build** | CMake 3.20+ + vcpkg | Gestão moderna de deps e builds |
| **CI/CD** | GitHub Actions | Build matrix (Win/Linux/macOS), testes, deploy |
| **Containers** | Docker + Kubernetes (AKS) | Orquestração, auto-scaling, self-healing |
| **API Gateway** | Envoy Proxy | gRPC-native, routing, rate-limiting, TLS |
| **Observabilidade** | OpenTelemetry + Prometheus + Grafana + Jaeger | Standard da indústria |
| **Data Stores** | PostgreSQL 16 + Redis 7 | Persistência (accounts, stats) + cache (sessions, leaderboards) |
| **Assets/CDN** | Azure Blob Storage + Azure CDN | Asset delivery global com baixa latência |

---

## 5. Mapeamento: Código Atual → Arquitetura-Alvo

```
CÓDIGO ATUAL                          DESTINO
─────────────────────────────────     ──────────────────────────

GAME SERVER (minimonolito — mesmo processo):
  QW/server/sv_main.c (loop)      ──→  services/gameserver/sv_loop.c
  QW/server/sv_phys.c             ──→  services/gameserver/physics/sv_phys.c
  QW/server/sv_move.c             ──→  services/gameserver/physics/sv_move.c
  QW/server/pr_exec.c             ──→  services/gameserver/gamelogic/libqcvm/
  QW/server/pr_cmds.c             ──→  services/gameserver/gamelogic/libqcvm/
  QW/server/pr_edict.c            ──→  services/gameserver/gamelogic/libqcvm/
  qw-qc/*.qc                      ──→  services/gameserver/gamelogic/progs/
  QW/server/sv_ents.c             ──→  services/gameserver/state/sv_ents.c
  QW/server/world.c               ──→  services/gameserver/state/sv_world.c
  QW/server/sv_send.c             ──→  services/gameserver/clients/sv_send.c
  QW/server/sv_user.c             ──→  services/gameserver/clients/sv_user.c

MICROSERVIÇOS PERIFÉRICOS:
  QW/server/sv_user.c (auth part) ──→  services/auth/
  WinQuake/net_main.c (master)    ──→  services/matchmaking/
  WinQuake/model.c, snd_mem.c     ──→  services/assets/
  Novo                            ──→  services/leaderboard/
  Novo                            ──→  services/telemetry/
  QW/server/sv_ccmds.c (admin)   ──→  API Gateway (admin routes)

CLIENTE:
  QW/client/cl_*.c                ──→  client/
  WinQuake/gl_*.c                 ──→  client/renderer/  (reescrito)
  WinQuake/snd_*.c                ──→  client/audio/     (OpenAL)
  WinQuake/in_win.c               ──→  client/input/     (SDL2)
  QW/client/net_chan.c            ──→  client/network/   (mantido)

REMOVIDO:
  WinQuake/r_*.c, d_*.c, d_*.s   ──→  Arquivado (software renderer)
  WinQuake/net_ipx.c, net_ser.c  ──→  Eliminado
  WinQuake/snd_gus.c, snd_dos.c  ──→  Eliminado
  WinQuake/vid_dos.c, dos_v2.c   ──→  Eliminado
```

---

## 6. Riscos & Mitigações

| # | Risco | Prob. | Impacto | Mitigação |
|---|-------|-------|---------|-----------|
| R1 | **Globals coupling** impede separação de subsistemas na Fase 1 | 🟡 Média | 🔴 Alto | Task 1.1 dedicada a eliminar globals. Abordagem ficheiro-a-ficheiro com testes. |
| R2 | **Renderer rewrite** demora mais que o esperado (Fase 3.1) | 🔴 Alta | 🟡 Médio | Começar com GL 3.3 (Fase 1.3) que já funciona. Vulkan é incremental por cima. |
| R3 | **Perda da "feel" original** do jogo ao modernizar | 🟡 Média | 🟡 Médio | Modo "Retro" nos shaders. Testes de gameplay A/B com versão original. Physics mantém tick rate original (77fps). |
| R4 | **Latência no control plane** (Auth, Matchmaking) afeta UX | 🟢 Baixa | 🟡 Médio | Estas operações são pre-game. Caching agressivo. JWT eliminates per-request auth calls. |
| R5 | **Game server pod networking** em K8s adiciona latência | 🟡 Média | 🔴 Alto | `hostNetwork: true` para game servers. Testar com iperf. Considerar bare-metal para torneios. |
| R6 | **Scope creep** nas features da Fase 5 | 🔴 Alta | 🟡 Médio | Gate reviews entre fases. Feature flags. MVP primeiro, polish depois. |
| R7 | **QuakeC VM isolation** (Fase 1.8) mais complexa que o esperado (builtins acoplados) | 🟡 Média | 🟡 Médio | Callback table pattern. Migrar builtins um a um com testes. |
| R8 | **Compatibilidade com mods** da comunidade quebra | 🟢 Baixa | 🟢 Baixo | Manter formato `.dat` do QuakeC como input suportado. VM mantém ABI. |

---

## 7. Definição de Done por Fase

| Fase | Critérios de Conclusão |
|------|----------------------|
| **Fase 0** | ✅ Código compila com CMake em Windows + Linux. CI/CD green. Testes smoke passam. Código legacy (DOS/IPX) removido. |
| **Fase 1** | ✅ Zero variáveis globais partilhadas entre subsistemas. PAL implementado com SDL2. Renderer GL 3.3 funcional. Áudio via OpenAL. Input via SDL2. Game loop separado (client vs server). `libqcvm` compila standalone. Interfaces `.h` definidas entre todos os subsistemas. Jogo funcional e idêntico ao original. |
| **Fase 2** | ✅ Game Server corre como binário containerizado. Auth Service valida JWT. Matchmaking lista servidores e faz queue. Asset Service serve ficheiros via HTTP+CDN. Eventos fluem via NATS. Jogo multiplayer funcional end-to-end com nova arquitetura. |
| **Fase 3** | ✅ Cliente modernizado com renderer Vulkan ou GL 4.6. UI via ImGui. Áudio espacial 3D. Login flow integrado. Cliente comunica com todos os serviços backend. Cross-platform (Win/Linux/macOS). |
| **Fase 4** | ✅ `docker compose up` funciona para dev local. Deploy automatizado em AKS via CI/CD. API Gateway com routing e auth. Observabilidade completa (dashboards + alertas). Game servers auto-escaláveis. |
| **Fase 5** | ✅ Player profiles persistentes. SBMM funcional. Pelo menos 2 features adicionais deployed. Sistema estável com jogadores reais. |

---

## 8. Próximos Passos

1. ~~**Fase 0 em curso** — Coding agent a trabalhar em CMake, CI/CD, compilação moderna~~
2. **Preparar Fase 1** — Criar issues detalhadas para cada sub-task (1.1 a 1.9)
3. **Spike técnico** — Prototipar PAL + SDL2 num branch de teste
4. **Spike técnico** — Testar extração de `libqcvm` (avaliar coupling real dos builtins)
5. **Review de arquitetura** com a equipa antes de iniciar Fase 2

---

> *"In the first age, in the first battle, when the shadows first lengthened..."*
> — Agora, modernizamos a lenda. 🚀
