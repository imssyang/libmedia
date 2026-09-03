add_executable(media_tests
    main.cpp
    test_media.cpp
)

target_link_libraries(media_tests
    PRIVATE
        media_static
        FFmpeg::avformat
)

target_include_directories(media_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(
    NAME media_tests
    COMMAND media_tests
)