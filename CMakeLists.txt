cmake_minimum_required(VERSION 3.22)

project(VocalEffectGaby VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# JUCE: se descarga automáticamente. Si ya lo tienes en tu compu, puedes usar
#   cmake -B build -DJUCE_DIR=/ruta/a/JUCE
# -----------------------------------------------------------------------------
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(GABY_FORMATS VST3)

juce_add_plugin(VocalEffectGaby
    COMPANY_NAME               "Gaby Records"
    COMPANY_WEBSITE            "https://gabyrecords.com"
    BUNDLE_ID                  com.gabyrecords.vocaleffectgaby
    PLUGIN_MANUFACTURER_CODE   Gaby
    PLUGIN_CODE                GbVx
    PRODUCT_NAME               "Vocal Effect Gaby"
    FORMATS                    ${GABY_FORMATS}
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            Fx Restoration
    AU_MAIN_TYPE               kAudioUnitType_Effect)

target_sources(VocalEffectGaby
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(VocalEffectGaby
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VocalEffectGaby
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
