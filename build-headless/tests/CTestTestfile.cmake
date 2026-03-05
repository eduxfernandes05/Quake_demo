# CMake generated Testfile for 
# Source directory: /home/runner/work/Quake_demo/Quake_demo/tests
# Build directory: /home/runner/work/Quake_demo/Quake_demo/build-headless/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(smoke_qwsv_exists "/usr/local/bin/cmake" "-E" "true")
set_tests_properties(smoke_qwsv_exists PROPERTIES  PASS_REGULAR_EXPRESSION "" REQUIRED_FILES "/home/runner/work/Quake_demo/Quake_demo/build-headless/QW/server/qwsv" _BACKTRACE_TRIPLES "/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;5;add_test;/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;0;")
add_test(smoke_quake_dedicated_exists "/usr/local/bin/cmake" "-E" "true")
set_tests_properties(smoke_quake_dedicated_exists PROPERTIES  PASS_REGULAR_EXPRESSION "" REQUIRED_FILES "/home/runner/work/Quake_demo/Quake_demo/build-headless/WinQuake/quake-dedicated" _BACKTRACE_TRIPLES "/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;14;add_test;/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;0;")
add_test(smoke_qwsv_runs "/usr/local/bin/cmake" "-E" "env" "HOME=/tmp" "/home/runner/work/Quake_demo/Quake_demo/build-headless/QW/server/qwsv" "-basedir" "/tmp" "-game" "nonexistent" "+quit")
set_tests_properties(smoke_qwsv_runs PROPERTIES  TIMEOUT "5" _BACKTRACE_TRIPLES "/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;23;add_test;/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;0;")
add_test(smoke_quake_dedicated_runs "/usr/local/bin/cmake" "-E" "env" "HOME=/tmp" "/home/runner/work/Quake_demo/Quake_demo/build-headless/WinQuake/quake-dedicated" "-basedir" "/tmp" "-game" "nonexistent")
set_tests_properties(smoke_quake_dedicated_runs PROPERTIES  TIMEOUT "5" WILL_FAIL "TRUE" _BACKTRACE_TRIPLES "/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;31;add_test;/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;0;")
add_test(smoke_headless_links_egl "sh" "-c" "ldd /home/runner/work/Quake_demo/Quake_demo/build-headless/WinQuake/quake-dedicated | grep libEGL")
set_tests_properties(smoke_headless_links_egl PROPERTIES  TIMEOUT "5" _BACKTRACE_TRIPLES "/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;41;add_test;/home/runner/work/Quake_demo/Quake_demo/tests/CMakeLists.txt;0;")
