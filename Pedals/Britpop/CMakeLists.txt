juce_add_plugin(Britpop
    COMPANY_NAME                "yourcompany"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE    Manu
    PLUGIN_CODE                 Xbzs
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Britpop")

target_sources(Britpop
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

juce_generate_juce_header(Britpop)

target_compile_definitions(Britpop
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Britpop
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
