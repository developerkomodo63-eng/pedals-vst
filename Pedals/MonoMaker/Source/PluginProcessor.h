#pragma once

#include <JuceHeader.h>

class MonoMakerAudioProcessor : public juce::AudioProcessor
{
public:
    MonoMakerAudioProcessor();
    ~MonoMakerAudioProcessor() override;

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
    // truco clasico de mastering: suma a mono todo lo que esta por debajo
    // del cruce (los graves en estereo suelen cancelarse en sistemas mono/
    // vinilo/club de todas formas), y deja el resto del espectro estereo
    // intacto. Cruce Linkwitz-Riley (fase-coherente), un filtro por canal.
    struct ChannelSplit
    {
        juce::dsp::LinkwitzRileyFilter<float> lowLP, highHP;
        void reset() { lowLP.reset(); highHP.reset(); }
    };
    std::vector<ChannelSplit> splits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoMakerAudioProcessor)
};
