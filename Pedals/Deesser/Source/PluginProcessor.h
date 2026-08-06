#pragma once

#include <JuceHeader.h>

class DeesserAudioProcessor : public juce::AudioProcessor
{
public:
    DeesserAudioProcessor();
    ~DeesserAudioProcessor() override;

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
    // de-esser "split-band": separa graves/medios de la banda de sibilancia
    // con un cruce Linkwitz-Riley (fase-coherente), comprime SOLO la banda
    // alta, y suma de vuelta. Asi no opaca toda la voz, solo la "s".
    struct ChannelSplit
    {
        juce::dsp::LinkwitzRileyFilter<float> lowLP, highHP;
        void reset() { lowLP.reset(); highHP.reset(); }
    };
    std::vector<ChannelSplit> splits;

    float envelopeDb = -100.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeesserAudioProcessor)
};
