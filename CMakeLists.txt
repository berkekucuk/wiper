cmake_minimum_required(VERSION 3.20)
project(winre-boot LANGUAGES CXX)

# ── Language standard ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Windows only
if(NOT WIN32)
    message(FATAL_ERROR "This project can only be built on Windows.")
endif()

# ── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/winre_finder.cpp
    src/countdown.cpp
    src/reboot.cpp
    src/winre_patcher.cpp
    src/disk_detector.cpp
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE include)

# ── Windows API libraries ─────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE
    advapi32    # Registry + AdjustTokenPrivileges
    user32      # ExitWindowsEx
)

# ── Windows macros (all compilers) ────────────────────────────────────────────
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")          # w64devkit / MinGW-w64 GCC
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall                  # Core warnings
        -Wextra                # Extra warnings
        -Wpedantic             # Standard conformance
        -Werror                # Treat warnings as errors
        -finput-charset=UTF-8  # Source file encoding
        -fexec-charset=UTF-8   # Executable output encoding
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
    )
endif()

# ── Build info ────────────────────────────────────────────────────────────────
message(STATUS "Project  : ${PROJECT_NAME}")
message(STATUS "Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Standard : C++${CMAKE_CXX_STANDARD}")
