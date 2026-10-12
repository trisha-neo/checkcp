cmake_minimum_required(VERSION 3.15)

# Oldest macOS the binary will run on. Must not be lower than what Homebrew built
# libcrypto.a for (15.0 for current bottles). Must precede project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS version")

project(checkcp VERSION 1.1.0 LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Produce a self-contained binary. On Linux it is fully static (-static) and runs on
# any distro regardless of its libc. On macOS only OpenSSL is linked statically: Apple
# does not support fully static binaries, so the result still links libSystem and
# libc++, which ship with every macOS install.
option(CHECKCP_STATIC "Build a self-contained binary" ON)

if(APPLE AND NOT OPENSSL_ROOT_DIR)
    # Homebrew's openssl@3 is keg-only, so point FindOpenSSL at it.
    execute_process(COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE _brew_openssl OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_brew_openssl)
        set(OPENSSL_ROOT_DIR ${_brew_openssl})
    endif()
endif()

if(CHECKCP_STATIC)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
find_package(OpenSSL REQUIRED)

# --- Vendored BLAKE3: pick SIMD sources by architecture ---
set(B3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/blake3)
set(B3_SOURCES
    ${B3_DIR}/blake3.c
    ${B3_DIR}/blake3_dispatch.c
    ${B3_DIR}/blake3_portable.c
)
set(B3_DEFINES "")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    # NEON is mandatory on ARMv8, so enable it unconditionally.
    list(APPEND B3_SOURCES ${B3_DIR}/blake3_neon.c)
    set(B3_DEFINES BLAKE3_USE_NEON=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    # x86-64 SIMD via assembly; dispatched at runtime.
    list(APPEND B3_SOURCES
        ${B3_DIR}/blake3_sse2_x86-64_unix.S
        ${B3_DIR}/blake3_sse41_x86-64_unix.S
        ${B3_DIR}/blake3_avx2_x86-64_unix.S
        ${B3_DIR}/blake3_avx512_x86-64_unix.S
    )
else()
    # Portable fallback: no SIMD.
    set(B3_DEFINES
        BLAKE3_NO_SSE2 BLAKE3_NO_SSE41 BLAKE3_NO_AVX2 BLAKE3_NO_AVX512 BLAKE3_NO_NEON)
endif()

add_library(blake3 STATIC ${B3_SOURCES})
target_include_directories(blake3 PRIVATE ${B3_DIR})
if(B3_DEFINES)
    target_compile_definitions(blake3 PRIVATE ${B3_DEFINES})
endif()

add_executable(checkcp
    src/main.cpp
    src/checksum.cpp
)

target_include_directories(checkcp PRIVATE src)
target_link_libraries(checkcp PRIVATE OpenSSL::Crypto blake3)

if(CHECKCP_STATIC AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Static libcrypto needs libdl and pthreads; they must follow it on the link line.
    find_package(Threads REQUIRED)
    target_link_libraries(checkcp PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    target_link_options(checkcp PRIVATE -static)
endif()

target_compile_options(checkcp PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:Release>:-O2>
)

install(TARGETS checkcp DESTINATION bin)
