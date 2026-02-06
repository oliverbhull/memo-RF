cmake_minimum_required(VERSION 3.15)
project(memo-rf VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Agent persona override (compile-time)
# Usage: cmake -DAGENT_PERSONA=manufacturing ..
if(AGENT_PERSONA)
    message(STATUS "Agent persona override: ${AGENT_PERSONA}")
    add_compile_definitions(AGENT_PERSONA_OVERRIDE="${AGENT_PERSONA}")
endif()

# Translation target language (compile-time)
# Usage: cmake -DTRANSLATE_LANGUAGE=Spanish ..
if(TRANSLATE_LANGUAGE)
    message(STATUS "Translation target language: ${TRANSLATE_LANGUAGE}")
    add_compile_definitions(TRANSLATE_LANGUAGE="${TRANSLATE_LANGUAGE}")
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Find required packages
find_package(PkgConfig REQUIRED)

# PortAudio for audio I/O
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# On macOS, add library search directories
if(APPLE)
    execute_process(
        COMMAND pkg-config --libs-only-L portaudio-2.0
        OUTPUT_VARIABLE PORTAUDIO_LIB_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Remove -L prefix and add to link directories
    string(REPLACE "-L" "" PORTAUDIO_LIB_DIRS "${PORTAUDIO_LIB_DIRS}")
    string(STRIP "${PORTAUDIO_LIB_DIRS}" PORTAUDIO_LIB_DIRS)
endif()

# JSON library (nlohmann/json via header-only)
# Try to find it in common locations first
find_path(JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS
    /usr/local/include
    /usr/include
    /opt/homebrew/include
    ${CMAKE_SOURCE_DIR}/third_party
)

if(NOT JSON_INCLUDE_DIR)
    message(STATUS "nlohmann/json not found.")
    message(STATUS "")
    message(STATUS "Please install nlohmann/json using one of these methods:")
    message(STATUS "  1. Homebrew: brew install nlohmann-json")
    message(STATUS "  2. Manual: Download json.hpp from:")
    message(STATUS "     https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp")
    message(STATUS "     and place it in: ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp")
    message(STATUS "")
    
    # Check if manual download exists
    set(MANUAL_JSON_PATH ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp)
    if(EXISTS ${MANUAL_JSON_PATH})
        message(STATUS "Found manually placed json.hpp at ${MANUAL_JSON_PATH}")
        set(JSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party)
    else()
        message(FATAL_ERROR "nlohmann/json.hpp not found. Please install it using one of the methods above.")
    endif()
else()
    message(STATUS "Found nlohmann/json at: ${JSON_INCLUDE_DIR}")
endif()

# whisper.cpp (assumes it's built and available)
# Allow user to specify path via -DWHISPER_DIR
if(WHISPER_DIR)
    set(WHISPER_POSSIBLE_PATHS ${WHISPER_DIR})
    message(STATUS "Using user-specified WHISPER_DIR: ${WHISPER_DIR}")
else()
    # Try multiple possible locations (whisper.cpp should be a sibling directory)
    set(WHISPER_POSSIBLE_PATHS
        ${CMAKE_SOURCE_DIR}/../whisper.cpp
        ${CMAKE_SOURCE_DIR}/../../whisper.cpp
        $ENV{HOME}/dev/whisper.cpp
        $ENV{HOME}/whisper.cpp
        /usr/local
        /opt/homebrew
    )
    # Linux / Jetson: add standard and user-local paths
    if(UNIX AND NOT APPLE)
        list(APPEND WHISPER_POSSIBLE_PATHS /usr /usr/local $ENV{HOME}/.local)
    endif()
    message(STATUS "Searching for whisper.cpp in: ${WHISPER_POSSIBLE_PATHS}")
endif()

find_path(WHISPER_INCLUDE_DIR whisper.h
    PATHS
    ${WHISPER_POSSIBLE_PATHS}
    PATH_SUFFIXES
    include
    .
    NO_DEFAULT_PATH
)

# Also check if whisper.h is directly in the path (some builds)
if(NOT WHISPER_INCLUDE_DIR)
    find_path(WHISPER_INCLUDE_DIR whisper.h
        PATHS
        ${WHISPER_POSSIBLE_PATHS}
        NO_DEFAULT_PATH
    )
endif()

# If not found, try default paths
if(NOT WHISPER_INCLUDE_DIR)
    find_path(WHISPER_INCLUDE_DIR whisper.h
        PATHS
        /usr/local/include
        /opt/homebrew/include
        /usr/include
    )
endif()

find_library(WHISPER_LIB 
    NAMES whisper libwhisper
    PATHS
    ${WHISPER_POSSIBLE_PATHS}
    PATH_SUFFIXES
    build/src
    src
    lib
    build
    build/lib
    .
    NO_DEFAULT_PATH
)

# If not found, try default paths
if(NOT WHISPER_LIB)
    find_library(WHISPER_LIB whisper
        PATHS
        /usr/local/lib
        /opt/homebrew/lib
        /usr/lib
    )
endif()

if(NOT WHISPER_INCLUDE_DIR)
    message(WARNING "whisper.h not found. Searched in: ${WHISPER_POSSIBLE_PATHS}")
    message(WARNING "Please ensure whisper.cpp is built and available.")
    message(WARNING "You can specify the path with: cmake -DWHISPER_DIR=/path/to/whisper.cpp ..")
    message(WARNING "Or if whisper is in ~/models/whisper, ensure it's built there.")
endif()

if(NOT WHISPER_LIB)
    message(WARNING "whisper library not found. Searched in: ${WHISPER_POSSIBLE_PATHS}")
    message(WARNING "Please ensure whisper.cpp is built: cd whisper.cpp && make")
    message(WARNING "You can specify the path with: cmake -DWHISPER_DIR=/path/to/whisper.cpp ..")
    message(WARNING "Or if whisper is in ~/models/whisper, ensure it's built there.")
endif()

if(NOT WHISPER_INCLUDE_DIR OR NOT WHISPER_LIB)
    message(FATAL_ERROR "")
    message(FATAL_ERROR "whisper.cpp not found. Please build whisper.cpp first.")
    message(FATAL_ERROR "")
    message(FATAL_ERROR "whisper.cpp is the SOURCE CODE repository, not the model files.")
    message(FATAL_ERROR "Model files (.bin) go in ~/models/whisper/, but you need the library too.")
    message(FATAL_ERROR "")
    message(FATAL_ERROR "To build whisper.cpp:")
    message(FATAL_ERROR "  1. Clone: git clone https://github.com/ggerganov/whisper.cpp.git")
    message(FATAL_ERROR "  2. Build: cd whisper.cpp && make")
    message(FATAL_ERROR "  3. Specify path: cmake -DWHISPER_DIR=/path/to/whisper.cpp ..")
    message(FATAL_ERROR "")
endif()

message(STATUS "Found whisper.h at: ${WHISPER_INCLUDE_DIR}")
message(STATUS "Found whisper library at: ${WHISPER_LIB}")

# Source files - Core (stripped: no core/config, core/types)
set(CORE_SOURCES
)

# Source files - VAD (stripped: use vad_endpointing only)
set(VAD_SOURCES
)

# Source files - TTS (stripped: use tts_engine only)
set(TTS_SOURCES
)

# Source files - Memory (stripped: no conversation_memory)
set(MEMORY_SOURCES
)

# Source files - main application
set(LEGACY_SOURCES
    src/main.cpp
    src/agent.cpp
    src/agent_pipeline.cpp
    src/audio_io.cpp
    src/path_utils.cpp
    src/vad_endpointing.cpp
    src/stt_engine.cpp
    src/router.cpp
    src/llm_client.cpp
    src/tts_engine.cpp
    src/tx_controller.cpp
    src/state_machine.cpp
    src/session_recorder.cpp
    src/config.cpp
    src/logger.cpp
)

# All sources
set(SOURCES
    ${CORE_SOURCES}
    ${VAD_SOURCES}
    ${TTS_SOURCES}
    ${MEMORY_SOURCES}
    ${LEGACY_SOURCES}
)

# Header files - Core (stripped)
set(CORE_HEADERS
)

# Header files - VAD (stripped)
set(VAD_HEADERS
)

# Header files - TTS (stripped)
set(TTS_HEADERS
)

# Header files - Memory (stripped)
set(MEMORY_HEADERS
)

# Header files - main application
set(LEGACY_HEADERS
    include/agent.h
    include/agent_pipeline.h
    include/audio_io.h
    include/vad_endpointing.h
    include/stt_engine.h
    include/router.h
    include/llm_client.h
    include/tts_engine.h
    include/tx_controller.h
    include/state_machine.h
    include/session_recorder.h
    include/config.h
    include/common.h
    include/logger.h
    include/utils.h
    include/transcript_gate.h
)

# All headers
set(HEADERS
    ${CORE_HEADERS}
    ${VAD_HEADERS}
    ${TTS_HEADERS}
    ${MEMORY_HEADERS}
    ${LEGACY_HEADERS}
)

# Executable
add_executable(memo-rf ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(memo-rf
    ${PORTAUDIO_LIBRARIES}
    ${WHISPER_LIB}
    pthread
    curl
)

# macOS only: link frameworks required for PortAudio. On Linux, no extra libraries needed.
if(APPLE)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(CORESERVICES_FRAMEWORK CoreServices)
    
    target_link_libraries(memo-rf
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${CORESERVICES_FRAMEWORK}
    )
endif()

# Find ggml include directory (whisper.h depends on it)
get_filename_component(WHISPER_BASE_DIR ${WHISPER_INCLUDE_DIR} DIRECTORY)
find_path(GGML_INCLUDE_DIR ggml.h
    PATHS
    ${WHISPER_BASE_DIR}/ggml/include
    ${WHISPER_BASE_DIR}/../ggml/include
    ${WHISPER_POSSIBLE_PATHS}
    PATH_SUFFIXES
    ggml/include
    include
    NO_DEFAULT_PATH
)

if(NOT GGML_INCLUDE_DIR)
    # Try default paths
    find_path(GGML_INCLUDE_DIR ggml.h
        PATHS
        /usr/local/include
        /opt/homebrew/include
        /usr/include
    )
endif()

target_include_directories(memo-rf PRIVATE
    ${PORTAUDIO_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIR}
    ${WHISPER_INCLUDE_DIR}
    ${GGML_INCLUDE_DIR}
)

target_compile_options(memo-rf PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

# Add PortAudio library directories
if(APPLE AND PORTAUDIO_LIB_DIRS)
    target_link_directories(memo-rf PRIVATE ${PORTAUDIO_LIB_DIRS})
endif()

# Install
install(TARGETS memo-rf DESTINATION bin)

# Deterministic test harness: gate + router only (no whisper/PortAudio)
add_executable(test_pipeline tests/test_pipeline.cpp src/router.cpp)
target_include_directories(test_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_pipeline)
