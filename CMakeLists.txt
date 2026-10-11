cmake_minimum_required(VERSION 3.22)

# Build a universal binary (Intel + Apple Silicon) on macOS. This must be set
# before project() — CMake locks in the target architecture(s) at that point,
# so setting it later has no effect on macOS.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS" FORCE)
endif()

project(RDSB VERSION 1.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure FetchContent to fetch JUCE
include(FetchContent)

# We use the stable juce8 branch or release tag
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)

# Tell JUCE not to build extras/examples to save compilation time
set(JUCE_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(JUCE_BUILD_EXTRAS OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(JUCE)

# Add the RDSB audio plugin target
juce_add_plugin(RDSB
    VERSION "1.1.0"
    COMPANY_NAME "Gary A. Newsome"
    COMPANY_COPYRIGHT "Gary A. Newsome"
    BUNDLE_ID "com.garyanewsome.RDSB"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "Gary"
    PLUGIN_CODE "RDSB"
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Rise.Drop.Sweep.Boom"
)

# Source files
target_sources(RDSB PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/TonalSweeper.h
    Source/NoiseEngine.h
    Source/BoomEngine.h
    Source/FastRandom.h
)

# Compile definitions
target_compile_definitions(RDSB PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Include directories
target_include_directories(RDSB PRIVATE
    Source
)

# Link JUCE modules and DSP library
target_link_libraries(RDSB PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
)

# Headless engine tests (not part of the default build):
#   cmake --build <dir> --target RDSBTests && <dir>/RDSBTests_artefacts/Release/RDSBTests
juce_add_console_app(RDSBTests PRODUCT_NAME "RDSBTests")
set_target_properties(RDSBTests PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(RDSBTests PRIVATE
    Tests/RDSBTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)
target_include_directories(RDSBTests PRIVATE Source)
target_compile_definitions(RDSBTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(RDSBTests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_recommended_config_flags
)
