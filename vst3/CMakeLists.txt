cmake_minimum_required(VERSION 3.22)

project(ANS1964VST3 VERSION 1.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/JUCE" CACHE PATH "JUCE source directory")
set(ANS_PLUGIN "${CMAKE_CURRENT_SOURCE_DIR}/../mpc")
set(ANS_DSP_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/mpc-sdk")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}. Run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${ANS_PLUGIN}/src/ANSDsp.h")
    message(FATAL_ERROR "ANSDsp.h not found at ${ANS_PLUGIN}/src")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_plugin(ANS1964
    COMPANY_NAME "bokontep"
    PRODUCT_NAME "ANS 1964"
    PLUGIN_MANUFACTURER_CODE Boko
    PLUGIN_CODE An64
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Instrument|Synth"
    BUNDLE_ID "com.bokontep.ans1964"
)

juce_generate_juce_header(ANS1964)
juce_add_binary_data(ANSData SOURCES "Assets/user.ansscore")

target_sources(ANS1964 PRIVATE
    Source/ANSParams.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(ANS1964 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_include_directories(ANS1964 PRIVATE
    "${ANS_DSP_HEADERS}"
    "${ANS_PLUGIN}/src"
    "${JUCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK"
)

target_link_libraries(ANS1964 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    ANSData
)
