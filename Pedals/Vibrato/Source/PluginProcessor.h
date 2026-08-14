#pragma once

#include <JuceHeader.h>

class VibratoAudioProcessor : public juce::AudioProcessor
{
public:
    VibratoAudioProcessor();
    ~VibratoAudioProcessor() override;

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
    // misma linea de delay modulada que el Chorus/Doubler/Tape, pero SIN
    // mezcla con la señal seca: 100% señal modulada. Sin mezcla no hay
    // filtrado tipo peine (eso es lo que distingue al vibrato del chorus,
    // que suena "grueso" justo por mezclar las dos)
    static constexpr double maxLineMs = 15.0;
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibratoAudioProcessor)
};
