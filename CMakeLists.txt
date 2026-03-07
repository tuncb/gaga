cmake_minimum_required(VERSION 3.20)

project(gaga LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

add_executable(gaga
    src/audio.cpp
    src/diagnostics.cpp
    src/file_watch.cpp
    src/main.cpp
    src/note.cpp
    src/parser.cpp
    src/pattern.cpp
    src/source_text.cpp
    src/synth.cpp
    src/tokenizer.cpp
    src/transport.cpp
)

target_include_directories(gaga PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${miniaudio_SOURCE_DIR}
)

if(WIN32)
    target_link_libraries(gaga PRIVATE avrt ole32 winmm)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(gaga_tests
        tests/parser_tests.cpp
        src/diagnostics.cpp
        src/note.cpp
        src/parser.cpp
        src/source_text.cpp
        src/tokenizer.cpp
    )

    target_include_directories(gaga_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )

    add_test(NAME gaga_tests COMMAND gaga_tests)
endif()
