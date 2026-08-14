#pragma once

#include <JuceHeader.h>

class BitcrusherAudioProcessor : public juce::AudioProcessor
{
public:
    BitcrusherAudioProcessor();
    ~BitcrusherAudioProcessor() override;

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
    // el DSP mas barato posible: sample & hold para "bajar" la tasa de
    // muestreo (nada de filtros/resampling real) y cuantizacion de
    // amplitud para "bajar" la profundidad de bits. Sin trigonometria,
    // sin transcendentales, sin estado mas que un contador y el ultimo
    // valor retenido, por canal.
    std::vector<float> heldSample;
    std::vector<int> holdCounter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BitcrusherAudioProcessor)
};
