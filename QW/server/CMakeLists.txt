# QuakeWorld Dedicated Server (qwsv)
#
# This is the dedicated server for QuakeWorld multiplayer.
# It has no graphics/audio dependencies, making it ideal for headless builds.

set(QWSV_SERVER_SOURCES
    pr_cmds.c
    pr_edict.c
    pr_exec.c
    sv_init.c
    sv_main.c
    sv_nchan.c
    sv_ents.c
    sv_send.c
    sv_move.c
    sv_phys.c
    sv_user.c
    sv_ccmds.c
    world.c
    sys_unix.c
    model.c
)

# Shared sources from QW/client/
set(QWSV_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/cmd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/cvar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/mathlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/md4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/zone.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/pmove.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/pmovetst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/net_chan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/net_udp.c
)

add_executable(qwsv
    ${QWSV_SERVER_SOURCES}
    ${QWSV_SHARED_SOURCES}
)

target_compile_definitions(qwsv PRIVATE SERVERONLY)

target_include_directories(qwsv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../client
)

target_link_libraries(qwsv PRIVATE m)

# Installation
install(TARGETS qwsv RUNTIME DESTINATION bin)
