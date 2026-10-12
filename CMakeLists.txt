cmake_minimum_required(VERSION 3.15)
project(checkcp VERSION 1.1.0 LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

target_compile_options(checkcp PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:Release>:-O2>
)

install(TARGETS checkcp DESTINATION bin)
