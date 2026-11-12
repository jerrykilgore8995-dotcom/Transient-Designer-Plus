cmake_minimum_required(VERSION 3.24)
project(TransientDesignerPlus VERSION 2.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
if(DEFINED JUCE_LOCAL_PATH)
  add_subdirectory("${JUCE_LOCAL_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
else()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.17-security GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(TransientDesignerPlus
  COMPANY_NAME "TDP Audio"
  BUNDLE_ID "audio.tdpaudio.transientdesignerplus"
  PLUGIN_MANUFACTURER_CODE TdPa
  PLUGIN_CODE TdP1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  FORMATS VST3 Standalone
  PRODUCT_NAME "Transient Designer Plus")
target_sources(TransientDesignerPlus PRIVATE
  Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(TransientDesignerPlus PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TransientDesignerPlus PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_executable(TdpDspTests tests/EngineTests.cpp)
target_include_directories(TdpDspTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
  target_compile_options(TdpDspTests PRIVATE /W4)
else()
  target_compile_options(TdpDspTests PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME TdpDspTests COMMAND TdpDspTests)

# Exact JUCE editor capture (not a concept illustration).
juce_add_console_app(TdpEditorPreview PRODUCT_NAME "TDP JUCE GUI Capture")
target_sources(TdpEditorPreview PRIVATE
  tests/EditorPreview.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(TdpEditorPreview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(TdpEditorPreview PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TdpEditorPreview PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# V2 DSP engine compiles standalone against C++17 for independent regression.
add_executable(TdpV2Tests tests/V2EngineTests.cpp)
target_include_directories(TdpV2Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
 target_compile_options(TdpV2Tests PRIVATE /W4)
else()
 target_compile_options(TdpV2Tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME TdpV2Tests COMMAND TdpV2Tests)
