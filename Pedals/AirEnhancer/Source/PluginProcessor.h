#pragma once

#include <JuceHeader.h>

class AirEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    AirEnhancerAudioProcessor();
    ~AirEnhancerAudioProcessor() override;

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
    // dos bandas de excitacion en paralelo, estilo excitador dual-banda:
    // Low agrega apertura/presencia en la zona de 1-3kHz,
    // High agrega aire por encima de los 10kHz. Cada banda es un
    // pasa-altos + saturacion suave, igual que el Exciter, pero acá con
    // dos frecuencias fijas en vez de una ajustable -- el objetivo es
    // "dos perillas simples", no un excitador de proposito general.
    juce::dsp::StateVariableTPTFilter<float> lowBandFilter;
    juce::dsp::StateVariableTPTFilter<float> highBandFilter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirEnhancerAudioProcessor)
};
