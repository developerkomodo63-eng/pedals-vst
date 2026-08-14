#pragma once

#include <JuceHeader.h>

class DoublerAudioProcessor : public juce::AudioProcessor
{
public:
    DoublerAudioProcessor();
    ~DoublerAudioProcessor() override;

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
    static constexpr double maxLineMs = 45.0;

    // ADT clasico (tecnica de doble grabacion artificial de los años 60):
    // una sola voz retardada con
    // modulacion de pitch sutil (LFO lento y poco profundo, mucho mas
    // discreto que un chorus) para simular una segunda toma sin que suene
    // a "efecto". Ancho estereo real: el canal derecho usa un delay base
    // distinto al izquierdo.
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoublerAudioProcessor)
};
