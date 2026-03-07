cmake_minimum_required(VERSION 3.20)

project(gaga LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d # miniaudio 0.11.25
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

add_executable(gaga
    src/audio.cpp
    src/audio_debug.cpp
    src/diagnostics.cpp
    src/file_watch.cpp
    src/main.cpp
    src/note.cpp
    src/parser.cpp
    src/pattern.cpp
    src/source_text.cpp
    src/synth.cpp
    src/terminal_display.cpp
    src/tokenizer.cpp
    src/transport.cpp
)

target_include_directories(gaga PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${miniaudio_SOURCE_DIR}
)

find_package(Threads REQUIRED)
target_link_libraries(gaga PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(gaga PRIVATE avrt ole32 winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(gaga PRIVATE dl m)
endif()

install(TARGETS gaga
    RUNTIME DESTINATION .
)

install(FILES README.md
    DESTINATION .
)

install(DIRECTORY examples/
    DESTINATION examples
)

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

    add_executable(gaga_audio_debug_tests
        tests/audio_debug_tests.cpp
        src/audio_debug.cpp
        src/note.cpp
        src/pattern.cpp
        src/synth.cpp
        src/transport.cpp
    )

    target_include_directories(gaga_audio_debug_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )

    add_test(NAME gaga_audio_debug_tests COMMAND gaga_audio_debug_tests)
endif()
