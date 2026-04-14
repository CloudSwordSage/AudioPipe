cmake_minimum_required(VERSION 3.15)
project(audio_pipe
  VERSION 0.1
  LANGUAGES CXX
  DESCRIPTION "AudioPipe - Audio processing Pipe"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SRC_FILES
    src/input/*.cpp
    src/logging/*.cpp
)

# Windows 平台：添加 WASAPI 实现 + 链接系统库
if(WIN32)
    file(GLOB_RECURSE WASAPI_FILES src/input/wasapi/*.cpp)
    list(APPEND SRC_FILES ${WASAPI_FILES})
    link_libraries(ole32 uuid propsys ksuser)
endif()

# 生成动态库
add_library(audio_pipe SHARED ${SRC_FILES})
