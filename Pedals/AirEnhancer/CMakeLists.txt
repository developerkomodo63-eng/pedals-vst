juce_add_plugin(AirEnhancer
    COMPANY_NAME                "DevKomodo"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE    Manu
    PLUGIN_CODE                 Air1
    FORMATS                     VST3
    PRODUCT_NAME                "Air Enhancer")

target_sources(AirEnhancer
    PRIVATE
        Source/PluginProcessor.cpp)

juce_generate_juce_header(AirEnhancer)

target_compile_definitions(AirEnhancer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AirEnhancer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
