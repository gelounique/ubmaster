cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
project(UBMaster VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---- JUCE: local checkout (-DJUCE_PATH=...) or fetched from GitHub ----
set(JUCE_PATH "" CACHE PATH "Path to a JUCE checkout")
if(JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- AAX (Pro Tools): needs the Avid AAX SDK (developer.avid.com) -> -DAAX_SDK_PATH=...
#      Release builds must additionally be signed with PACE wraptool, otherwise Pro Tools only loads them in developer mode.
set(AAX_SDK_PATH "" CACHE PATH "Path to the Avid AAX SDK")
set(UBMASTER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND UBMASTER_FORMATS AU)
endif()
if(AAX_SDK_PATH AND EXISTS "${AAX_SDK_PATH}/Interfaces")
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
    list(APPEND UBMASTER_FORMATS AAX)
    message(STATUS "UBMaster: AAX enabled (${AAX_SDK_PATH})")
endif()

juce_add_plugin(UBMaster
    PRODUCT_NAME "UBMaster"
    COMPANY_NAME "UNIQUEBABY"
    COMPANY_WEBSITE "https://ubmaster.uniquebaby.de"
    COMPANY_EMAIL "support@uniquebaby.de"
    COMPANY_COPYRIGHT "(c) 2026 unique & MELO - UNIQUEBABY"
    BUNDLE_ID de.uniquebaby.ubmaster
    PLUGIN_MANUFACTURER_CODE Unqb
    PLUGIN_CODE UbMs
    DESCRIPTION "UBMaster - mastering suite by unique & MELO"
    FORMATS ${UBMASTER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_SANDBOX_SAFE TRUE
    AAX_CATEGORY AAX_ePlugInCategory_Dynamics
    HARDENED_RUNTIME_ENABLED TRUE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/branding/icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/branding/icon-256.png")

juce_add_binary_data(UBMasterData SOURCES
    plugin/resources/ub-logo.png
    plugin/resources/fonts/InstrumentSans-Regular.ttf
    plugin/resources/fonts/InstrumentSans-SemiBold.ttf
    plugin/resources/fonts/InstrumentSans-Bold.ttf
    plugin/resources/fonts/JetBrainsMono-Regular.ttf
    plugin/resources/fonts/JetBrainsMono-Medium.ttf)

set(UBMASTER_SOURCES
    plugin/source/PluginProcessor.cpp
    plugin/source/PluginEditor.cpp
    plugin/source/ui/Theme.cpp
    plugin/source/ui/Widgets.cpp
    plugin/source/ui/Halo.cpp
    plugin/source/ui/Panels.cpp)

target_sources(UBMaster PRIVATE ${UBMASTER_SOURCES})
target_include_directories(UBMaster PRIVATE plugin/source)

target_compile_definitions(UBMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    UBMASTER_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(UBMaster
    PRIVATE
        UBMasterData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- tests + headless UI snapshot (website screenshots / CI smoke test) ----
option(UBMASTER_BUILD_TESTS "Build DSP tests and the UI snapshot tool" OFF)
if(UBMASTER_BUILD_TESTS)
    juce_add_console_app(UBMasterTests PRODUCT_NAME "UBMasterTests")
    target_sources(UBMasterTests PRIVATE tests/dsp_test.cpp plugin/source/PluginProcessor.cpp plugin/source/PluginEditor.cpp
        plugin/source/ui/Theme.cpp plugin/source/ui/Widgets.cpp plugin/source/ui/Halo.cpp plugin/source/ui/Panels.cpp)
    juce_add_console_app(UBMasterSnapshot PRODUCT_NAME "UBMasterSnapshot")
    target_sources(UBMasterSnapshot PRIVATE tests/snapshot.cpp ${UBMASTER_SOURCES})
    foreach(t UBMasterTests UBMasterSnapshot)
        target_include_directories(${t} PRIVATE plugin/source)
        target_compile_definitions(${t} PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            JucePlugin_Name="UBMaster" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
            JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0
            UBMASTER_VERSION_STRING="${PROJECT_VERSION}")
        target_link_libraries(${t} PRIVATE UBMasterData juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_extra
            juce::juce_recommended_config_flags)
    endforeach()
endif()
