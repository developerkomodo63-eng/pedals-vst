juce_add_plugin(Overdrive
    COMPANY_NAME                "DevKomodo"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE    Manu
    PLUGIN_CODE                 Xbzs
    FORMATS                     VST3
    PRODUCT_NAME                "Overdrive")

target_sources(Overdrive
    PRIVATE
        Source/PluginProcessor.cpp)

juce_generate_juce_header(Overdrive)

target_compile_definitions(Overdrive
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Overdrive
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
