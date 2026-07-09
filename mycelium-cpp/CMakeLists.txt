cmake_minimum_required(VERSION 3.20)
project(mycelium VERSION 0.01 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mycelium
    src/main.cpp
)

target_include_directories(mycelium PRIVATE src)

if(NOT MYCELIUM_BUILD_VERSION)
    set(MYCELIUM_BUILD_VERSION "0.01")
endif()
target_compile_definitions(mycelium PRIVATE MYCELIUM_VERSION="${MYCELIUM_BUILD_VERSION}")

if(WIN32)
    target_link_libraries(mycelium PRIVATE ws2_32 bcrypt)
endif()
