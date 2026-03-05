# Quake — Arquitetura Atual (C4 Model)

Este documento descreve a arquitetura do código-fonte original do Quake
utilizando o modelo C4 (Context, Container, Component).

---

## Nível 1 — Contexto do Sistema

```
┌──────────────┐          UDP/IPX            ┌──────────────┐
│   Jogador    │ ◄──────────────────────────► │   Quake      │
│  (Utilizador)│          (Rede)             │   Server     │
└──────────────┘                             └──────┬───────┘
                                                    │
                                             ┌──────▼───────┐
                                             │  File System  │
                                             │  (pak files,  │
                                             │   maps, mdl)  │
                                             └──────────────┘
```

**Descrição:** O jogador interage com o Quake através de um cliente que comunica
com um servidor de jogo via protocolo UDP. O servidor carrega assets (maps, modelos,
sons) do sistema de ficheiros local.

---

## Nível 2 — Containers

```
┌─────────────────────────────────────────────────────────────────┐
│                        QUAKE SYSTEM                             │
│                                                                 │
│  ┌─────────────────────┐         ┌─────────────────────┐       │
│  │   WinQuake Engine   │  UDP    │  QuakeWorld Server   │       │
│  │   (Cliente + Svr)   │◄──────►│  (qwsv)              │       │
│  │                     │         │  Dedicated server    │       │
│  │  • Rendering (SW/GL)│         │  • Game logic        │       │
│  │  • Audio (DMA)      │         │  • Physics           │       │
│  │  • Input (KB/Mouse) │         │  • Networking        │       │
│  │  • Networking       │         │  • QuakeC VM         │       │
│  │  • QuakeC VM        │         └─────────────────────┘       │
│  └─────────────────────┘                                        │
│                                                                 │
│  ┌─────────────────────┐                                        │
│  │   QuakeWorld Client │                                        │
│  │   (qwcl)            │                                        │
│  │  • Client-side pred │                                        │
│  │  • Rendering        │                                        │
│  │  • Audio            │                                        │
│  └─────────────────────┘                                        │
│                                                                 │
│  ┌─────────────────────┐                                        │
│  │   QuakeC Scripts    │                                        │
│  │   (qw-qc/*.qc)     │                                        │
│  │  • Game rules       │                                        │
│  │  • Weapons/Items    │                                        │
│  │  • Player logic     │                                        │
│  └─────────────────────┘                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## Nível 3 — Componentes (WinQuake Engine)

```
┌─────────────────────────────────────────────────────────────────┐
│                     WinQuake Engine                              │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  Game Loop   │  │   QuakeC VM  │  │  Command System      │  │
│  │  host.c      │  │  pr_exec.c   │  │  cmd.c, cvar.c       │  │
│  │  host_cmd.c  │  │  pr_edict.c  │  │  console.c           │  │
│  │              │  │  pr_cmds.c   │  │                      │  │
│  └──────┬───────┘  └──────┬───────┘  └──────────────────────┘  │
│         │                 │                                     │
│  ┌──────▼─────────────────▼──────────────────────────────────┐  │
│  │                  Server Subsystem                          │  │
│  │  sv_main.c  sv_phys.c  sv_move.c  sv_user.c  world.c     │  │
│  └───────────────────────┬───────────────────────────────────┘  │
│                          │                                      │
│  ┌───────────┐  ┌────────▼───┐  ┌───────────┐  ┌───────────┐  │
│  │ Rendering │  │ Networking │  │   Audio    │  │   Input   │  │
│  │           │  │            │  │            │  │           │  │
│  │ r_main.c  │  │ net_main.c │  │ snd_dma.c  │  │ in_*.c   │  │
│  │ r_draw.c  │  │ net_dgrm.c │  │ snd_mem.c  │  │          │  │
│  │ r_surf.c  │  │ net_udp.c  │  │ snd_mix.c  │  │          │  │
│  │ d_*.c     │  │ net_loop.c │  │ snd_*.c    │  │          │  │
│  │ gl_*.c    │  │            │  │            │  │          │  │
│  └───────────┘  └────────────┘  └───────────┘  └───────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │           Platform Abstraction (tightly coupled)         │   │
│  │  sys_linux.c / sys_win.c    vid_*.c    cd_*.c            │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Nível 3 — Componentes (QuakeWorld Server)

```
┌─────────────────────────────────────────────────────────────────┐
│                   QuakeWorld Server (qwsv)                       │
│                                                                 │
│  ┌──────────────────┐    ┌──────────────────────────────────┐   │
│  │  Main Loop       │    │  Client Management               │   │
│  │  sv_main.c       │    │  sv_user.c   sv_ccmds.c          │   │
│  │  sys_unix.c      │    │  sv_send.c   sv_nchan.c          │   │
│  └────────┬─────────┘    └──────────────┬───────────────────┘   │
│           │                             │                       │
│  ┌────────▼──────────────────────────────▼──────────────────┐   │
│  │                  Game Simulation                          │   │
│  │                                                          │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │   │
│  │  │ Entity Mgmt  │  │   Physics    │  │  QuakeC VM   │   │   │
│  │  │ sv_ents.c    │  │  sv_phys.c   │  │  pr_exec.c   │   │   │
│  │  │ sv_init.c    │  │  sv_move.c   │  │  pr_edict.c  │   │   │
│  │  │              │  │              │  │  pr_cmds.c   │   │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘   │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  Networking  │  │  World/BSP   │  │  Shared Libraries    │  │
│  │  net_chan.c  │  │  world.c     │  │  common.c  mathlib.c │  │
│  │  net_udp.c  │  │  model.c     │  │  zone.c    cvar.c    │  │
│  │             │  │              │  │  cmd.c     crc.c     │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │          Player Movement Prediction                      │   │
│  │  pmove.c    pmovetst.c                                   │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Ficheiros-Chave e Responsabilidades

| Ficheiro | Componente | Responsabilidade |
|----------|-----------|------------------|
| `host.c` | Game Loop | Loop principal, frame timing, inicialização |
| `sv_main.c` | Server | Frame loop do servidor, gestão de clientes |
| `sv_phys.c` | Physics | Simulação de física, colisões, gravidade |
| `sv_move.c` | Physics | Movimento de entidades (AI pathfinding) |
| `pr_exec.c` | QuakeC VM | Execução de bytecode QuakeC |
| `pr_edict.c` | QuakeC VM | Gestão de entidades (edicts) |
| `pr_cmds.c` | QuakeC VM | Funções built-in chamadas pelo QuakeC |
| `world.c` | World | BSP traversal, hull clipping |
| `model.c` | Assets | Loading de .bsp, .mdl, .spr |
| `net_main.c` | Networking | Gestão de conexões de rede |
| `net_chan.c` | Networking | Reliable/unreliable channels (QW) |
| `net_udp.c` | Networking | Transporte UDP |
| `common.c` | Shared | Parsing, file I/O, memória |
| `zone.c` | Shared | Alocação de memória (zone/hunk) |
| `mathlib.c` | Shared | Operações vectoriais e matriciais |

---

## Dependências entre Componentes

```
Game Loop (host.c)
    ├── Server (sv_main.c)
    │     ├── Physics (sv_phys.c, sv_move.c)
    │     ├── Entity Management (sv_ents.c, sv_init.c)
    │     ├── QuakeC VM (pr_exec.c, pr_edict.c, pr_cmds.c)
    │     ├── World (world.c)
    │     └── Networking (net_main.c, net_chan.c, net_udp.c)
    ├── Client (cl_main.c, cl_parse.c, cl_input.c)
    │     ├── Rendering (r_main.c, d_*.c / gl_*.c)
    │     ├── Audio (snd_dma.c, snd_mem.c, snd_mix.c)
    │     └── Input (in_*.c)
    └── Shared (common.c, zone.c, mathlib.c, cvar.c, cmd.c)
```

---

> **Nota:** Esta documentação reflete a arquitetura *antes* da modernização.
> Consultar `MODERNIZATION_PLAN.md` para a arquitetura-alvo de microserviços.
