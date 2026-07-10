cmake_minimum_required(VERSION 3.20)
project(mycelium VERSION 0.01 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable compiler extensions for GCC/Clang only — MSVC /Za breaks Windows headers
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

add_executable(mycelium
    src/main.cpp
)

target_include_directories(mycelium PRIVATE src)

if(NOT MYCELIUM_BUILD_VERSION)
    set(MYCELIUM_BUILD_VERSION "0.01")
endif()
target_compile_definitions(mycelium PRIVATE MYCELIUM_VERSION="${MYCELIUM_BUILD_VERSION}")

# MSVC: suppress POSIX deprecation warnings (snprintf, fopen, strtok)
if(MSVC)
    target_compile_definitions(mycelium PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

find_package(Threads REQUIRED)
target_link_libraries(mycelium PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(mycelium PRIVATE ws2_32 bcrypt gdi32 comctl32)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(mycelium PRIVATE OpenSSL::Crypto)
endif()
