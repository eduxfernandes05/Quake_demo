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

# Test 3: qwsv starts, initializes and shuts down cleanly
add_test(NAME smoke_qwsv_runs
    COMMAND ${CMAKE_COMMAND} -E env "HOME=/tmp" $<TARGET_FILE:qwsv> -basedir /tmp -game nonexistent +quit
)
set_tests_properties(smoke_qwsv_runs PROPERTIES
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

# Test 5: When HEADLESS=ON, verify the headless binary links EGL/GL/GBM
if(HEADLESS)
    add_test(NAME smoke_headless_links_egl
        COMMAND sh -c "ldd $<TARGET_FILE:quake-dedicated> | grep libEGL"
    )
    set_tests_properties(smoke_headless_links_egl PROPERTIES
        TIMEOUT 5
    )
endif()
