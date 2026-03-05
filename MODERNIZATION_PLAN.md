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

## 2. Arquitetura-Alvo: Microserviços

```
                    ┌──────────────────┐
                    │   API Gateway    │
                    │   (REST/gRPC)    │
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
   ┌────▼─────┐       ┌─────▼──────┐      ┌─────▼──────┐
   │  Auth &   │       │  Match-    │      │  Player    │
   │  Session  │       │  making    │      │  Profile   │
   │  Service  │       │  Service   │      │  Service   │
   └───────────┘       └─────┬──────┘      └────────────┘
                             │
                    ┌────────▼─────────┐
                    │   Game Server    │
                    │   Orchestrator   │
                    └────────┬─────────┘
                             │
          ┌──────────────────┼──────────────────┐
          │                  │                  │
   ┌──────▼───────┐  ┌──────▼───────┐  ┌──────▼───────┐
   │  Game State  │  │   Physics    │  │   Game       │
   │  Service     │  │   Service    │  │   Logic      │
   │  (entities,  │  │  (sv_phys)   │  │   Service    │
   │   world)     │  │              │  │  (QuakeC VM) │
   └──────────────┘  └──────────────┘  └──────────────┘
          │                  │                  │
          └──────────────────┼──────────────────┘
                             │
                    ┌────────▼─────────┐
                    │   Event Bus      │
                    │  (Message Queue) │
                    └────────┬─────────┘
                             │
          ┌──────────────────┼──────────────────┐
          │                  │                  │
   ┌──────▼───────┐  ┌──────▼───────┐  ┌──────▼───────┐
   │   Asset      │  │  Telemetry   │  │  Leaderboard │
   │   Service    │  │  & Logging   │  │  Service     │
   │  (maps, mdl) │  │  Service     │  │              │
   └──────────────┘  └──────────────┘  └──────────────┘

   ┌──────────────────────────────────────────────────┐
   │              CLIENTE (Modernizado)                │
   │  ┌──────────┐ ┌──────────┐ ┌──────────────────┐  │
   │  │ Vulkan / │ │ Modern   │ │ Cross-platform   │  │
   │  │ OpenGL   │ │ Audio    │ │ Input (SDL2)     │  │
   │  │ Renderer │ │ (OpenAL) │ │                  │  │
   │  └──────────┘ └──────────┘ └──────────────────┘  │
   └──────────────────────────────────────────────────┘
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

| # | Task | Ficheiros Impactados | Prioridade |
|---|------|---------------------|-----------|
| 1.1 | Criar **Platform Abstraction Layer (PAL)** — interfaces para vídeo, som, input, rede | `pal/video.h`, `pal/audio.h`, `pal/input.h`, `pal/network.h` (novos) | 🔴 Alta |
| 1.2 | Migrar rendering para SDL2 + OpenGL 3.3 Core (remover software renderer e GL 1.x legacy) | `gl_*.c` → `renderer/` (refactor) | 🔴 Alta |
| 1.3 | Migrar áudio para OpenAL ou SDL_mixer | `snd_*.c` → `audio/` (refactor) | 🟡 Média |
| 1.4 | Migrar input para SDL2 | `in_win.c`, `in_dos.c` → `input/` (refactor) | 🟡 Média |
| 1.5 | Migrar networking para sockets modernos (remover IPX/serial, manter apenas UDP) | `net_*.c` → `network/` (refactor) | 🟡 Média |
| 1.6 | Separar game loop do servidor e do cliente em módulos independentes | `host.c`, `sv_main.c`, `cl_main.c` | 🔴 Alta |
| 1.7 | Isolar a QuakeC VM como biblioteca standalone (`libqcvm`) | `pr_exec.c`, `pr_edict.c`, `pr_cmds.c`, `pr_comp.h` | 🟡 Média |
| 1.8 | Definir interfaces claras (headers) entre subsistemas | Novos `.h` de interface | 🟡 Média |

---

### Fase 2 — Extração de Microserviços (Backend)
> **Objetivo:** Extrair componentes server-side em serviços independentes com comunicação via gRPC/mensagens.

| # | Task | Ficheiros-Origem | Serviço-Alvo | Prioridade |
|---|------|-----------------|-------------|-----------|
| 2.1 | **Auth & Session Service** — Autenticação de jogadores, gestão de sessões, tokens JWT | `sv_user.c` (connect/disconnect), `sv_ccmds.c` | `services/auth/` | 🔴 Alta |
| 2.2 | **Game State Service** — Gestão de entidades, mundo, BSP data | `sv_ents.c`, `sv_init.c`, `world.c`, `pr_edict.c` | `services/gamestate/` | 🔴 Alta |
| 2.3 | **Physics Service** — Simulação de física, colisões, movimento | `sv_phys.c`, `sv_move.c` | `services/physics/` | 🔴 Alta |
| 2.4 | **Game Logic Service** — QuakeC VM como serviço (executa lógica de jogo) | `pr_exec.c`, `pr_cmds.c`, `qw-qc/*.qc` | `services/gamelogic/` | 🔴 Alta |
| 2.5 | **Matchmaking Service** — Lobby, matchmaking, server browser | `net_main.c` (master server queries) | `services/matchmaking/` | 🟡 Média |
| 2.6 | **Asset Service** — Servir maps (.bsp), modelos (.mdl), sons (.wav), texturas | `model.c`, `gl_model.c`, `snd_mem.c` | `services/assets/` | 🟡 Média |
| 2.7 | **Leaderboard & Stats Service** — Rankings, estatísticas de jogadores | Novo (dados de `combat.qc`, `weapons.qc`) | `services/leaderboard/` | 🟢 Baixa |
| 2.8 | **Telemetry & Logging Service** — Métricas de performance, logs de jogo | Novo | `services/telemetry/` | 🟢 Baixa |
| 2.9 | **Game Server Orchestrator** — Criar/destruir instâncias de servidores de jogo | `sv_main.c` (frame loop) | `services/orchestrator/` | 🟡 Média |

---

### Fase 3 — Modernização do Cliente
> **Objetivo:** Modernizar o cliente mantendo a gameplay original.

| # | Task | Descrição | Prioridade |
|---|------|-----------|-----------|
| 3.1 | Reescrever renderer com Vulkan/OpenGL 4.6 (manter estética retro como opção) | Novo pipeline gráfico | 🔴 Alta |
| 3.2 | Implementar sistema de UI moderno (ImGui ou similar para menus/HUD) | Substituir `menu.c`, `sbar.c` | 🟡 Média |
| 3.3 | Adicionar suporte a resoluções modernas, widescreen, HDR | Configuração de vídeo | 🟡 Média |
| 3.4 | Implementar áudio espacial 3D moderno (OpenAL Soft) | Substituir `snd_dma.c` | 🟡 Média |
| 3.5 | Cross-platform builds (Windows, Linux, macOS) com SDL2 | Build system | 🟡 Média |
| 3.6 | Cliente comunica com backend via gRPC/WebSocket em vez de protocolo legacy | `cl_parse.c`, `cl_input.c`, `net_chan.c` | 🔴 Alta |

---

### Fase 4 — Infraestrutura & DevOps
> **Objetivo:** Suportar a arquitetura de microserviços com infraestrutura cloud-native.

| # | Task | Descrição | Prioridade |
|---|------|-----------|-----------|
| 4.1 | Containerizar cada microserviço (Docker) | `Dockerfile` por serviço | 🔴 Alta |
| 4.2 | Orquestração com Kubernetes (AKS ou similar) | Helm charts / manifests | 🔴 Alta |
| 4.3 | Implementar API Gateway (gRPC gateway ou Envoy) | Roteamento, rate-limiting, auth | 🔴 Alta |
| 4.4 | Event Bus / Message Queue (RabbitMQ, NATS ou Azure Service Bus) | Comunicação assíncrona entre serviços | 🟡 Média |
| 4.5 | Observabilidade — distributed tracing (OpenTelemetry), métricas (Prometheus), logs (ELK) | Stack de monitoring | 🟡 Média |
| 4.6 | Base de dados por serviço (PostgreSQL para profiles, Redis para game state) | Data stores | 🟡 Média |
| 4.7 | Auto-scaling de game servers baseado em carga | HPA / KEDA | 🟢 Baixa |

---

### Fase 5 — Novas Funcionalidades
> **Objetivo:** Aproveitar a nova arquitetura para adicionar funcionalidades impossíveis no monolito.

| # | Task | Descrição | Prioridade |
|---|------|-----------|-----------|
| 5.1 | Sistema de accounts e perfis de jogador persistentes | Auth + Profile services | 🟡 Média |
| 5.2 | Matchmaking inteligente (skill-based) | Matchmaking service | 🟡 Média |
| 5.3 | Replay system (gravação server-side de partidas) | Game State service | 🟢 Baixa |
| 5.4 | Mod support via API (carregar game logic mods como plugins) | Game Logic service API | 🟢 Baixa |
| 5.5 | Spectator mode melhorado com streaming | Client + Game State | 🟢 Baixa |
| 5.6 | Anti-cheat server-side | Auth + Physics services | 🟡 Média |

---

## 4. Stack Tecnológica Proposta

| Camada | Tecnologia | Justificação |
|--------|-----------|-------------|
| **Linguagem (serviços)** | C++ 20 / Rust | Performance para game server; Rust para serviços não-críticos |
| **Linguagem (cliente)** | C++ 20 | Compatibilidade com libs gráficas |
| **Comunicação síncrona** | gRPC (Protocol Buffers) | Baixa latência, tipagem forte |
| **Comunicação assíncrona** | NATS / Azure Service Bus | Event-driven entre serviços |
| **Rendering** | Vulkan + SDL2 | Cross-platform, moderno |
| **Áudio** | OpenAL Soft | 3D spatial audio, cross-platform |
| **Build** | CMake + Conan/vcpkg | Gestão de dependências moderna |
| **CI/CD** | GitHub Actions | Integrado com o repo |
| **Containers** | Docker + Kubernetes (AKS) | Orquestração cloud-native |
| **Observabilidade** | OpenTelemetry + Prometheus + Grafana | Stack standard da indústria |
| **Base de dados** | PostgreSQL + Redis | Persistência + cache de game state |

---

## 5. Mapeamento: Código Atual → Microserviços

```
CÓDIGO ATUAL                          MICROSERVIÇO ALVO
─────────────────────────────────     ──────────────────────────
QW/server/sv_user.c (connect)    ──→  Auth & Session Service
QW/server/sv_init.c              ──→  Game State Service
QW/server/sv_ents.c              ──→  Game State Service
QW/server/world.c                ──→  Game State Service
QW/server/sv_phys.c              ──→  Physics Service
QW/server/sv_move.c              ──→  Physics Service
QW/server/pr_exec.c              ──→  Game Logic Service (QuakeC VM)
QW/server/pr_cmds.c              ──→  Game Logic Service
QW/server/pr_edict.c             ──→  Game Logic Service
qw-qc/*.qc                      ──→  Game Logic Service (scripts)
QW/server/sv_main.c (loop)      ──→  Game Server Orchestrator
QW/server/sv_send.c              ──→  Game State Service (broadcast)
QW/server/sv_ccmds.c             ──→  API Gateway (admin commands)
QW/client/net_chan.c             ──→  Networking Library (shared)
WinQuake/net_main.c (master)     ──→  Matchmaking Service
WinQuake/model.c, gl_model.c    ──→  Asset Service
WinQuake/snd_mem.c               ──→  Asset Service
QW/client/cl_*.c                 ──→  Cliente Modernizado
WinQuake/gl_*.c, r_*.c          ──→  Cliente (Renderer)
WinQuake/snd_*.c                 ──→  Cliente (Audio)
```

---

## 6. Riscos & Mitigações

| Risco | Impacto | Mitigação |
|-------|---------|-----------|
| **Latência entre serviços** afeta gameplay (physics + game state separados) | 🔴 Alto | Co-localizar serviços críticos; usar shared memory quando no mesmo nó |
| **Complexidade de sincronização** de game state distribuído | 🔴 Alto | Padrão Event Sourcing; single source of truth no Game State Service |
| **Perda da "feel" original** do jogo ao modernizar | 🟡 Médio | Testes A/B com gameplay original; manter modo "classic" |
| **Scope creep** — adicionar features não planeadas | 🟡 Médio | Seguir rigorosamente as fases; reviews por fase |
| **Performance do QuakeC VM** como serviço remoto | 🟡 Médio | Manter VM in-process no game server; expor API para extensibilidade |
| **Compatibilidade com mods** existentes da comunidade | 🟢 Baixo | Manter formato `.dat` do QuakeC como input suportado |

---

## 7. Definição de Done por Fase

| Fase | Critério de Conclusão |
|------|----------------------|
| **Fase 0** | Código compila com CMake em Windows + Linux; CI green; testes smoke passam |
| **Fase 1** | Jogo funcional com SDL2/OpenGL 3.3; subsistemas com interfaces definidas; zero dependências de plataformas legacy |
| **Fase 2** | Cada serviço corre independentemente em container; comunicação via gRPC funcional; jogo multiplayer funcional com nova arquitetura |
| **Fase 3** | Cliente modernizado liga-se ao backend de microserviços; rendering Vulkan funcional; cross-platform |
| **Fase 4** | Deploy automatizado em Kubernetes; observabilidade completa; auto-scaling testado |
| **Fase 5** | Novas funcionalidades deployed e testadas com jogadores reais |

---

## 8. Próximos Passos

1. **Criar issues no GitHub** para cada task das Fases 0 e 1
2. **Atribuir prioridades** e estimar esforço (story points)
3. **Iniciar Fase 0.1** — Setup CMake build system
4. **Review de arquitetura** com a equipa antes de avançar para Fase 2

---

> *"In the first age, in the first battle, when the shadows first lengthened..."*
> — Agora, modernizamos a lenda. 🚀
