cmake_minimum_required(VERSION 3.10)

project(libmedia LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(ROOT_DIR "${CMAKE_SOURCE_DIR}")
set(TEST_DIR "${ROOT_DIR}/tests")
set(INSTALL_DIR "${ROOT_DIR}/release")
set(FFMPEG_HOME "${workspaceFolder}/deps/ffmpeg-8.0")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
else()
    message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O0")

include_directories(${FFMPEG_HOME}/include)
link_directories(${FFMPEG_HOME}/lib)

set(FFMPEG_LIBS
    avcodec
    avformat
    avutil
    swscale
    swresample
)

set(SOURCES
    ffmpeg.cpp
    avmanage.cpp
    avmedia.cpp
    avformat.cpp
    avcodec.cpp
    avutil.cpp
    swscale.cpp
    swresample.cpp
)

add_library(media_static STATIC ${SOURCES})
set_target_properties(media_static PROPERTIES OUTPUT_NAME "media")
target_link_libraries(media_static ${FFMPEG_LIBS})

add_library(media_shared SHARED ${SOURCES})
set_target_properties(media_shared PROPERTIES OUTPUT_NAME "media")
if(APPLE)
    set_target_properties(media_shared PROPERTIES
        INSTALL_NAME_DIR "@rpath"
        INSTALL_RPATH "@rpath"
    )
endif()
target_link_libraries(media_shared ${FFMPEG_LIBS})

file(GLOB TEST_SOURCES "${TEST_DIR}/*.cpp")
add_executable(media_test ${TEST_SOURCES})
target_include_directories(media_test PRIVATE .)
target_link_libraries(media_test media_static ${FFMPEG_LIBS})

install(DIRECTORY ${ROOT_DIR}/
    DESTINATION ${INSTALL_DIR}/include
    FILES_MATCHING PATTERN "*.h"
    PATTERN "build" EXCLUDE
    PATTERN "release" EXCLUDE
    PATTERN "tests" EXCLUDE
    PATTERN "CMakeFiles" EXCLUDE
)
install(TARGETS media_static media_shared
    ARCHIVE DESTINATION ${INSTALL_DIR}/lib
    LIBRARY DESTINATION ${INSTALL_DIR}/lib
    RUNTIME DESTINATION ${INSTALL_DIR}/bin
)

add_custom_target(run_test COMMAND ./media_test)
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/CMakeFiles/CMakeClean.cmake
    COMMAND ${CMAKE_COMMAND} --build . --target clean
)
