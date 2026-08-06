#pragma once

#include <JuceHeader.h>
#include <array>

class GraphicEQAudioProcessor : public juce::AudioProcessor
{
public:
    GraphicEQAudioProcessor();
    ~GraphicEQAudioProcessor() override;

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

    // frecuencias centrales ISO estandar (grilla de 1 octava, 31Hz a 16kHz)
    static constexpr std::array<float, 10> bandFrequencies {
        31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };

private:
    static constexpr int numBands = 10;

    struct ChannelBands
    {
        std::array<juce::dsp::IIR::Filter<float>, numBands> filters;
        void reset() { for (auto& f : filters) f.reset(); }
    };
    std::vector<ChannelBands> bands;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicEQAudioProcessor)
};
