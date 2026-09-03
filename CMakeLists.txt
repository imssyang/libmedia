cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0144 NEW)

project(libmedia
    VERSION 1.0.0
    LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# Options
# ---------------------------------------------------------

option(MEDIA_BUILD_TESTS "Build media tests" ON)

# ---------------------------------------------------------
# Install prefix
#
# Default:
#
#   <project>/release
#
# Can still be overridden:
#
#   cmake -B build -DCMAKE_INSTALL_PREFIX=/some/path
# ---------------------------------------------------------

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(
        CMAKE_INSTALL_PREFIX
        "${CMAKE_CURRENT_SOURCE_DIR}/release"
        CACHE PATH "Install path" FORCE
    )
endif()

message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# ---------------------------------------------------------
# CMake modules
# ---------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

# ---------------------------------------------------------
# FFmpeg
# ---------------------------------------------------------

set(
    FFMPEG_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg-7.1"
    CACHE PATH "FFmpeg root directory"
)

find_package(FFmpeg REQUIRED)

# ---------------------------------------------------------
# Sources
# ---------------------------------------------------------

set(MEDIA_SOURCES
    src/media.cpp
    src/avmanage.cpp
    src/avmedia.cpp
    src/avformat.cpp
    src/avcodec.cpp
    src/avutil.cpp
    src/swscale.cpp
    src/swresample.cpp
)

# ---------------------------------------------------------
# Static library
# ---------------------------------------------------------

add_library(media_static STATIC
    ${MEDIA_SOURCES}
)

add_library(media::static ALIAS media_static)

set_target_properties(media_static PROPERTIES
    OUTPUT_NAME media
)

target_compile_features(media_static
    PUBLIC cxx_std_20
)

target_compile_options(media_static
    PRIVATE
        -Wall
        -Wextra
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
)

target_include_directories(media_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(media_static
    PRIVATE
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::avutil
        FFmpeg::swscale
        FFmpeg::swresample
)

# ---------------------------------------------------------
# Shared library
# ---------------------------------------------------------

add_library(media_shared SHARED
    ${MEDIA_SOURCES}
)

add_library(media::shared ALIAS media_shared)

set_target_properties(media_shared PROPERTIES
    OUTPUT_NAME media
)

target_compile_features(media_shared
    PUBLIC cxx_std_20
)

target_compile_options(media_shared
    PRIVATE
        -Wall
        -Wextra
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
)

target_include_directories(media_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(media_shared
    PRIVATE
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::avutil
        FFmpeg::swscale
        FFmpeg::swresample
)

# ---------------------------------------------------------
# Runtime search path
# ---------------------------------------------------------

if(APPLE)

    set_target_properties(media_shared PROPERTIES
        BUILD_RPATH "${FFMPEG_LIBRARY_DIR}"
        INSTALL_NAME_DIR "@rpath"
        INSTALL_RPATH "@loader_path"
    )

endif()

if(UNIX AND NOT APPLE)

    set_target_properties(media_shared PROPERTIES
        BUILD_RPATH "${FFMPEG_LIBRARY_DIR}"
        INSTALL_RPATH "$ORIGIN"
    )

endif()

# ---------------------------------------------------------
# Tests
# ---------------------------------------------------------

if(MEDIA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------
# Install
# ---------------------------------------------------------

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Libraries
install(
    TARGETS
        media_static
        media_shared

    EXPORT
        libmediaTargets

    ARCHIVE DESTINATION
        ${CMAKE_INSTALL_LIBDIR}

    LIBRARY DESTINATION
        ${CMAKE_INSTALL_LIBDIR}

    RUNTIME DESTINATION
        ${CMAKE_INSTALL_BINDIR}

    INCLUDES DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
)

# Public headers
install(
    DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/include/"

    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
)

# CMake package
install(
    EXPORT
        libmediaTargets

    FILE
        libmediaTargets.cmake

    NAMESPACE
        libmedia::

    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/libmedia
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libmediaConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libmediaConfig.cmake"

    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/libmedia
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/libmediaConfigVersion.cmake"

    VERSION
        ${PROJECT_VERSION}

    COMPATIBILITY
        SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindFFmpeg.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/libmediaConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/libmediaConfigVersion.cmake"

    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/libmedia
)
