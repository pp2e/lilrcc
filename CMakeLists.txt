cmake_minimum_required(VERSION 3.16)

project(lilrcc VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
find_package(zstd)

add_library(lilrcc SHARED
    lilrcc.h lilrcc.cpp
    resourcereader.h resourcereader.cpp
    tree.h tree.cpp
    resourcewriter.h resourcewriter.cpp
)

target_link_libraries(lilrcc
    PRIVATE Qt6::Core
    PRIVATE zstd
)

qt_add_executable(lilrcc_cli
    main.cpp
)

target_link_libraries(lilrcc_cli
    PRIVATE lilrcc
)
