include(FindPackageHandleStandardArgs)

# ---------------------------------------------------------------------------
# FFmpeg root
#
# User can specify:
#
#   cmake -DFFMPEG_ROOT=/opt/homebrew/opt/ffmpeg ..
#
# Or set FFMPEG_ROOT in the parent CMakeLists.txt.
# ---------------------------------------------------------------------------

if(NOT FFMPEG_ROOT)
    message(FATAL_ERROR
        "FFMPEG_ROOT must be specified, e.g. "
        "-DFFMPEG_ROOT=/opt/homebrew/opt/ffmpeg"
    )
else()
    message(STATUS "Using FFmpeg root: ${FFMPEG_ROOT}")
endif()

# =========================================================
# FFmpeg components
# =========================================================

set(_FFMPEG_COMPONENTS
    avformat
    avcodec
    avutil
    swscale
    swresample
)

# =========================================================
# Find FFmpeg include directory
# =========================================================

find_path(FFMPEG_INCLUDE_DIR
    NAMES
        libavutil/avutil.h
        libavcodec/avcodec.h
        libavformat/avformat.h
    PATHS
        "${FFMPEG_ROOT}"
    PATH_SUFFIXES
        include
    NO_DEFAULT_PATH
)

# =========================================================
# Find FFmpeg libraries
# =========================================================

foreach(_component IN LISTS _FFMPEG_COMPONENTS)

    find_library(
        FFMPEG_${_component}_LIBRARY
        NAMES
            ${_component}
            lib${_component}
        PATHS
            "${FFMPEG_ROOT}"
        PATH_SUFFIXES
            lib
        NO_DEFAULT_PATH
    )

endforeach()


# =========================================================
# Check required variables
# =========================================================

set(_FFMPEG_REQUIRED_VARS
    FFMPEG_INCLUDE_DIR
)

foreach(_component IN LISTS _FFMPEG_COMPONENTS)

    list(APPEND
        _FFMPEG_REQUIRED_VARS
        FFMPEG_${_component}_LIBRARY
    )

endforeach()

find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS
        ${_FFMPEG_REQUIRED_VARS}

    VERSION_VAR
        FFMPEG_VERSION
)

# =========================================================
# Determine FFmpeg library directory
# =========================================================

if(FFmpeg_FOUND)

    get_filename_component(
        FFMPEG_LIBRARY_DIR
        "${FFMPEG_avformat_LIBRARY}"
        DIRECTORY
    )

endif()

# ---------------------------------------------------------------------------
# Imported targets
# ---------------------------------------------------------------------------

if(FFmpeg_FOUND)

    foreach(_component IN LISTS _FFMPEG_COMPONENTS)

        if(NOT TARGET FFmpeg::${_component})

            add_library(
                FFmpeg::${_component}
                UNKNOWN IMPORTED
            )

            set_target_properties(
                FFmpeg::${_component}
                PROPERTIES
                    IMPORTED_LOCATION
                        "${FFMPEG_${_component}_LIBRARY}"

                    INTERFACE_INCLUDE_DIRECTORIES
                        "${FFMPEG_INCLUDE_DIR}"
            )

        endif()

    endforeach()

endif()

# ---------------------------------------------------------------------------
# Advanced variables
# ---------------------------------------------------------------------------

mark_as_advanced(
    FFMPEG_ROOT
    FFMPEG_INCLUDE_DIR
    FFMPEG_LIBRARY_DIR
    FFMPEG_avutil_LIBRARY
    FFMPEG_swresample_LIBRARY
    FFMPEG_swscale_LIBRARY
    FFMPEG_avcodec_LIBRARY
    FFMPEG_avformat_LIBRARY
)

