cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

project(VoxSlap VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

if(APPLE)
    set(VOXSLAP_FORMATS AU VST3 Standalone)
else()
    set(VOXSLAP_FORMATS VST3 Standalone)
endif()

option(VOXSLAP_COPY_AFTER_BUILD "Install plugins into the system plug-in folders after building" ON)

juce_add_plugin(VoxSlap
    COMPANY_NAME "Homebrew Audio"
    PLUGIN_MANUFACTURER_CODE Hbau
    PLUGIN_CODE Vxsl
    FORMATS ${VOXSLAP_FORMATS}
    PRODUCT_NAME "VoxSlap"
    BUNDLE_ID "com.homebrewaudio.voxslap"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${VOXSLAP_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

# Photo-based UI textures (baked from CC0 ambientCG scans by tools/make_textures.py)
juce_add_binary_data(VoxSlapAssets SOURCES
    Resources/panel_burgundy.jpg
    Resources/panel_black.jpg
    Resources/plate_aluminium.jpg
    Resources/wood_cheeks.jpg
    Resources/glass_smudges.jpg)

target_sources(VoxSlap PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp)

target_compile_definitions(VoxSlap PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VoxSlap
    PRIVATE
        VoxSlapAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test tool: renders test audio through every preset and saves a UI screenshot.
option(VOXSLAP_BUILD_TOOLS "Build the offline render/screenshot tool" OFF)
if(VOXSLAP_BUILD_TOOLS)
    juce_add_console_app(VoxSlapRender PRODUCT_NAME "VoxSlapRender")
    target_sources(VoxSlapRender PRIVATE tools/Render.cpp)
    target_link_libraries(VoxSlapRender PRIVATE VoxSlap juce::juce_audio_utils juce::juce_dsp)
endif()
