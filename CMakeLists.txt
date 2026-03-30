cmake_minimum_required(VERSION 3.20)
project(Same_Blue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    src/main.cpp
    src/core_math.cpp
    src/hdr/hdr_probe.cpp
    src/encode/pipeline.cpp
)

if(WIN32)
    message(STATUS "Configuring for Windows")
    set(PLATFORM_SOURCES src/platform/win32_dx11.cpp)

    link_libraries(d3d11 dxgi d3dcompiler)

    add_definitions(-DNOMINMAX)

elseif(APPLE)
    message(STATUS "Configuring for macOS")
    set(PLATFORM_SOURCES src/platform/macos_metal.cpp)

    add_compile_options(-Wall)

    # FFmpeg integration for macOS
    set(CMAKE_PREFIX_PATH
        "/opt/homebrew;/usr/local"
        ${CMAKE_PREFIX_PATH}
    )

    # Find required FFmpeg components
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(FFMPEG REQUIRED
        libavformat>=58.0
        libavcodec>=58.0
        libavutil>=56.0
        libswscale>=5.0
    )

    if(FFMPEG_FOUND)
        message(STATUS "Found FFmpeg libraries:")
        message(STATUS "  libavformat: ${FFMPEG_libavformat_VERSION}")
        message(STATUS "  libavcodec: ${FFMPEG_libavcodec_VERSION}")
        message(STATUS "  libavutil: ${FFMPEG_libavutil_VERSION}")

        # Add FFmpeg include directories and libraries
        include_directories(${FFMPEG_INCLUDE_DIRS})
        link_directories(${FFMPEG_LIBRARY_DIRS})

        add_definitions(${FFMPEG_CFLAGS_OTHER})
        set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES})
    else()
        message(FATAL_ERROR "FFmpeg not found. Please install via: brew install ffmpeg")
    endif()
endif()

add_executable(Same_Blue ${COMMON_SOURCES} ${PLATFORM_SOURCES})
# Link FFmpeg libraries on macOS
if(APPLE AND FFMPEG_FOUND)
    target_link_libraries(Same_Blue ${FFMPEG_LIBRARIES})
endif()

# Unit test executable (no FFmpeg dependency)
add_executable(Same_Blue_tests src/tests/color_convert_test.cpp)
target_include_directories(Same_Blue_tests PRIVATE src)
