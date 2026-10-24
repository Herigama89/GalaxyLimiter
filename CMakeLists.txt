cmake_minimum_required(VERSION 3.22)

# These two must come BEFORE project(), otherwise CMake ignores them and builds
# only for the build machine's own chip and macOS version.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary: Apple Silicon + Intel")

project(GalaxyLimiter VERSION 1.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GalaxyLimiter
    COMPANY_NAME                "YourCompany"
    BUNDLE_ID                   "com.yourcompany.galaxylimiter"
    PLUGIN_MANUFACTURER_CODE    Ycmp
    PLUGIN_CODE                 Glxl
    FORMATS                     VST3 AU Standalone      # AU is only built on macOS
    PRODUCT_NAME                "Galaxy Limiter"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_CATEGORIES             Fx Dynamics Mastering
    AU_MAIN_TYPE                kAudioUnitType_Effect
)

target_sources(GalaxyLimiter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(GalaxyLimiter PRIVATE Source)

target_compile_definitions(GalaxyLimiter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(GalaxyLimiter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
