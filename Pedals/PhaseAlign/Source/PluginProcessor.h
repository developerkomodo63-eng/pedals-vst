#pragma once

#include <JuceHeader.h>

class PhaseAlignAudioProcessor : public juce::AudioProcessor
{
public:
    PhaseAlignAudioProcessor();
    ~PhaseAlignAudioProcessor() override;

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
    // sin sidechain: se usa por oido, insertado en (por ej.) el bajo,
    // mientras escuchas contra el bombo en la mezcla normal. Delay fino
    // (ms) + inversion de polaridad para el choque de tiempo, y un
    // rotador de fase (allpass de 2do orden, tecnica clasica de rotador
    // de fase de hardware) para
    // el choque de fase que un simple delay no arregla sin correr todo.
    static constexpr double maxDelayMs = 10.0;
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    juce::dsp::IIR::Filter<float> rotatorL, rotatorR;

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseAlignAudioProcessor)
};
