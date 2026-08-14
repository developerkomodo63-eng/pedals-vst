#pragma once

#include <JuceHeader.h>

class ExciterAudioProcessor : public juce::AudioProcessor
{
public:
    ExciterAudioProcessor();
    ~ExciterAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    // excitador clasico de una sola banda: pasa-altos, satura esa parte
    // (genera armonicos que el oido interpreta como "brillo"/presencia),
    // y la suma EN PARALELO con la señal original (nunca reemplaza el
    // agudo real, solo le agrega encima)
    juce::dsp::StateVariableTPTFilter<float> hpFilter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterAudioProcessor)
};
