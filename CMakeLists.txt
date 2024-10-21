cmake_minimum_required(VERSION 3.16)

project(lilrcc VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
find_package(zstd)
if(${zstd_FOUND})
    message("Found zstd")
    set(LILRCC_ZSTD_TARGET zstd)
else()
    message("Cannot find zstd, downloading")
    include(FetchContent)

    set(ZSTD_BUILD_STATIC ON)
    set(ZSTD_BUILD_SHARED OFF)

    FetchContent_Declare(
        zstd
        URL "https://github.com/facebook/zstd/releases/download/v1.5.5/zstd-1.5.5.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR build/cmake
    )

    FetchContent_MakeAvailable(zstd)

    set(LILRCC_ZSTD_TARGET libzstd_static)
endif()

add_library(lilrcc STATIC
    error.h error.cpp
    lilrcc.h lilrcc.cpp
    resourcereader.h resourcereader.cpp
    tree.h tree.cpp
    resourcewriter.h resourcewriter.cpp
)

target_link_libraries(lilrcc
    PRIVATE Qt6::Core
    PRIVATE ${LILRCC_ZSTD_TARGET}
)

qt_add_executable(lilrcc_cli
    main.cpp
)

target_link_libraries(lilrcc_cli
    PRIVATE lilrcc
)
