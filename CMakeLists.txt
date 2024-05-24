cmake_minimum_required(VERSION 3.16)

project(lilrcc VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
find_package(zstd)

qt_add_executable(lilrcc
    main.cpp
    lilrcc.h lilrcc.cpp
    lilrccreader.h lilrccreader.cpp
    tree.h tree.cpp
)

target_link_libraries(lilrcc
    PRIVATE zstd
)
