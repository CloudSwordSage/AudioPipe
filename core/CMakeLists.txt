cmake_minimum_required(VERSION 3.15)

project(audio_pipe
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "AudioPipe - Audio processing Pipe"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(AUDIO_PIPE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(AUDIO_PIPE_DIST_DIR ${AUDIO_PIPE_ROOT_DIR}/dist)
set(AUDIO_PIPE_DIST_INCLUDE_DIR ${AUDIO_PIPE_DIST_DIR}/include)

set(AUDIO_PIPE_SOURCES
    src/abi/ap_c_api.cpp
    src/input/input.cpp
    src/logging/logger.cpp
)

if(WIN32)
    list(APPEND AUDIO_PIPE_SOURCES
        src/input/wasapi/wasapi_input.cpp
    )
endif()

add_library(audio_pipe SHARED ${AUDIO_PIPE_SOURCES})

if(MSVC)
    target_compile_options(audio_pipe PRIVATE /utf-8)

    target_compile_options(audio_pipe PRIVATE
        $<$<CONFIG:Release>:/O2;/GL;/GF;/Gy>
        $<$<CONFIG:RelWithDebInfo>:/O2;/GL;/GF;/Gy>
    )

    target_link_options(audio_pipe PRIVATE
        $<$<CONFIG:Release>:/LTCG;/OPT:REF;/OPT:ICF>
        $<$<CONFIG:RelWithDebInfo>:/LTCG;/OPT:REF;/OPT:ICF>
    )
elseif(APPLE)
    target_compile_options(audio_pipe PRIVATE
        $<$<CONFIG:Release>:-O2;-flto;-ffunction-sections;-fdata-sections>
        $<$<CONFIG:RelWithDebInfo>:-O2;-flto;-ffunction-sections;-fdata-sections>
    )

    target_link_options(audio_pipe PRIVATE
        $<$<CONFIG:Release>:-flto;-Wl,-dead_strip>
        $<$<CONFIG:RelWithDebInfo>:-flto;-Wl,-dead_strip>
    )
else()
    target_compile_options(audio_pipe PRIVATE
        $<$<CONFIG:Release>:-O2;-flto;-ffunction-sections;-fdata-sections>
        $<$<CONFIG:RelWithDebInfo>:-O2;-flto;-ffunction-sections;-fdata-sections>
    )

    target_link_options(audio_pipe PRIVATE
        $<$<CONFIG:Release>:-flto;-Wl,--gc-sections>
        $<$<CONFIG:RelWithDebInfo>:-flto;-Wl,--gc-sections>
    )
endif()

target_compile_definitions(audio_pipe
    PRIVATE AUDIO_PIPE_BUILD_DLL
)

target_include_directories(audio_pipe
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/exported
)

if(WIN32)
    target_link_libraries(audio_pipe
        PRIVATE
            ole32
            uuid
            propsys
            ksuser
    )
endif()

set_target_properties(audio_pipe PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

add_custom_command(TARGET audio_pipe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${AUDIO_PIPE_DIST_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${AUDIO_PIPE_DIST_INCLUDE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/include/exported
            ${AUDIO_PIPE_DIST_INCLUDE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:audio_pipe>
            ${AUDIO_PIPE_DIST_DIR}
)
