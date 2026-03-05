# WinQuake — Original Quake Engine
#
# Builds the dedicated server (headless, no graphics/audio dependencies).
# Uses null drivers for video, sound, input, and CD audio.

# --------------------------------------------------------------------------
# Dedicated Server (quake-dedicated)
# Uses null drivers — no graphics, no audio, no input hardware
# --------------------------------------------------------------------------

set(QUAKE_CORE_SOURCES
    # Client
    chase.c
    cl_demo.c
    cl_input.c
    cl_main.c
    cl_parse.c
    cl_tent.c

    # Command & console
    cmd.c
    common.c
    console.c
    crc.c
    cvar.c

    # Rendering (software)
    d_edge.c
    d_fill.c
    d_init.c
    d_modech.c
    d_part.c
    d_polyse.c
    d_scan.c
    d_sky.c
    d_sprite.c
    d_surf.c
    d_vars.c
    d_zpoint.c
    draw.c
    r_aclip.c
    r_alias.c
    r_bsp.c
    r_draw.c
    r_edge.c
    r_efrag.c
    r_light.c
    r_main.c
    r_misc.c
    r_part.c
    r_sky.c
    r_sprite.c
    r_surf.c
    r_vars.c
    screen.c
    sbar.c
    view.c

    # Host & server
    host.c
    host_cmd.c
    sv_main.c
    sv_phys.c
    sv_move.c
    sv_user.c

    # World & model
    mathlib.c
    model.c
    world.c
    wad.c
    zone.c

    # QuakeC VM
    pr_cmds.c
    pr_edict.c
    pr_exec.c

    # Networking (UDP only — no legacy IPX/serial)
    net_bsd.c
    net_dgrm.c
    net_loop.c
    net_main.c
    net_udp.c
    net_vcr.c

    # Input, sound, CD — null stubs
    in_null.c
    snd_null.c
    cd_null.c

    # Keyboard & menu
    keys.c
    menu.c

    # Misc
    nonintel.c
)

# --------------------------------------------------------------------------
# Headless video driver option
# When ON, use vid_headless.c (EGL + LLVMpipe off-screen rendering)
# When OFF, use vid_null.c (stub — no rendering)
# --------------------------------------------------------------------------
option(HEADLESS "Build with headless EGL video driver for frame capture" OFF)

if(HEADLESS)
    set(VID_SOURCE vid_headless.c)
else()
    set(VID_SOURCE vid_null.c)
endif()

set(QUAKE_PLATFORM_SOURCES
    sys_linux.c
)

add_executable(quake-dedicated
    ${QUAKE_CORE_SOURCES}
    ${VID_SOURCE}
    ${QUAKE_PLATFORM_SOURCES}
)

target_compile_definitions(quake-dedicated PRIVATE
    _DEFAULT_SOURCE
)

# FNDELAY is a legacy BSD constant removed from modern glibc.
# Map it to the POSIX-equivalent O_NDELAY if the system header
# does not provide it.
include(CheckSymbolExists)
check_symbol_exists(FNDELAY "fcntl.h" HAVE_FNDELAY)
if(NOT HAVE_FNDELAY)
    target_compile_definitions(quake-dedicated PRIVATE FNDELAY=O_NDELAY)
endif()

target_include_directories(quake-dedicated PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quake-dedicated PRIVATE m)

# Headless mode requires EGL, GL, and GBM for off-screen rendering
if(HEADLESS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
    pkg_check_modules(GBM REQUIRED IMPORTED_TARGET gbm)
    find_package(OpenGL REQUIRED)
    target_link_libraries(quake-dedicated PRIVATE
        OpenGL::GL PkgConfig::EGL PkgConfig::GBM
    )
    target_compile_definitions(quake-dedicated PRIVATE HEADLESS)
endif()

# Installation
install(TARGETS quake-dedicated RUNTIME DESTINATION bin)
