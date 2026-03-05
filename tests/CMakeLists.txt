# Smoke Tests
# Phase 0.4 — Verify the code compiles and executables are produced

# Test 1: qwsv binary was built
add_test(NAME smoke_qwsv_exists
    COMMAND ${CMAKE_COMMAND} -E true
)
set_tests_properties(smoke_qwsv_exists PROPERTIES
    PASS_REGULAR_EXPRESSION ""
    REQUIRED_FILES "$<TARGET_FILE:qwsv>"
)

# Test 2: quake-dedicated binary was built
add_test(NAME smoke_quake_dedicated_exists
    COMMAND ${CMAKE_COMMAND} -E true
)
set_tests_properties(smoke_quake_dedicated_exists PROPERTIES
    PASS_REGULAR_EXPRESSION ""
    REQUIRED_FILES "$<TARGET_FILE:quake-dedicated>"
)

# Test 3: qwsv responds to --help / prints version (exits with error since no game data, but doesn't crash)
add_test(NAME smoke_qwsv_runs
    COMMAND ${CMAKE_COMMAND} -E env "HOME=/tmp" $<TARGET_FILE:qwsv>  -basedir /tmp -game nonexistent +quit
)
# The server will exit with an error because there's no game data, but it should at least start
set_tests_properties(smoke_qwsv_runs PROPERTIES
    WILL_FAIL TRUE
    TIMEOUT 5
)

# Test 4: quake-dedicated starts (will fail without game data, but verifies it doesn't segfault instantly)
add_test(NAME smoke_quake_dedicated_runs
    COMMAND ${CMAKE_COMMAND} -E env "HOME=/tmp" $<TARGET_FILE:quake-dedicated> -basedir /tmp -game nonexistent
)
set_tests_properties(smoke_quake_dedicated_runs PROPERTIES
    WILL_FAIL TRUE
    TIMEOUT 5
)
