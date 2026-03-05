cmake_minimum_required(VERSION 3.16)
project(Quake VERSION 1.09 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common compiler flags for modern compilers
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wno-unused-result
        -Wno-pointer-sign
        -Wno-stringop-truncation
        -Wno-format-truncation
        -Dstricmp=strcasecmp
    )

    # Legacy C code uses tentative definitions (multiple definitions of the
    # same global variable across translation units).  GCC 10+ defaults to
    # -fno-common, which turns these into linker errors.  Restore the old
    # behaviour so the original code links correctly.
    add_compile_options(-fcommon)

    # Suppress warnings common in legacy C code
    add_compile_options(
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-misleading-indentation
        -Wno-parentheses
        -Wno-missing-braces
    )
elseif(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3 /wd4244 /wd4127 /wd4201 /wd4214 /wd4514 /wd4305 /wd4115 /wd4018)
endif()

# Build targets
add_subdirectory(QW)
add_subdirectory(WinQuake)

# Enable testing
enable_testing()
add_subdirectory(tests)
