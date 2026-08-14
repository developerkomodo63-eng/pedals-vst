#pragma once

#include <JuceHeader.h>

class ChorusAudioProcessor : public juce::AudioProcessor
{
public:
    ChorusAudioProcessor();
    ~ChorusAudioProcessor() override;

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
    static constexpr float maxCentreDelayMs = 30.0f;
    static constexpr float maxModMs = 6.0f;
    static constexpr double maxLineMs = maxCentreDelayMs + maxModMs + 5.0;

    // una sola linea de delay por canal; las "voces" del chorus son varios
    // taps de lectura modulados leyendo de la misma linea, no lineas
    // separadas, asi el costo de memoria no crece con la cantidad de voces
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    double sampleRate = 44100.0;
    float masterPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusAudioProcessor)
};
