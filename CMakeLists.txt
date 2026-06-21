cmake_minimum_required(VERSION 3.15)
project(checkcp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

add_executable(checkcp
    src/main.cpp
    src/checksum.cpp
)

target_include_directories(checkcp PRIVATE src)
target_link_libraries(checkcp PRIVATE OpenSSL::Crypto)

target_compile_options(checkcp PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:Release>:-O2>
)

install(TARGETS checkcp DESTINATION bin)
