cmake_minimum_required(VERSION 3.16)

project(lilrcc VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_package(zstd)

qt_add_executable(lilrcc
    main.cpp
    rcc.cpp
    rcc.h
)

target_link_libraries(lilrcc
    PRIVATE zstd
)
