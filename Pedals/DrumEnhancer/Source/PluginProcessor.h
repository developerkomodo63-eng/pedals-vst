#pragma once

#include <JuceHeader.h>

class DrumEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    DrumEnhancerAudioProcessor();
    ~DrumEnhancerAudioProcessor() override;

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
    // Excitador armonico multibanda clasico: separa graves/agudos con un
    // cruce Linkwitz-Riley, agrega saturacion suave (distinta por banda)
    // EN PARALELO con la señal seca, y suma. Aclaracion honesta: los
    // excitadores multibanda de hardware clasicos tambien suelen hacer
    // alineacion de fase/tiempo entre bandas; aca lo omitimos por
    // simplicidad, esto se queda con la parte que mas define el caracter
    // (excitacion armonica por banda).
    struct ChannelSplit
    {
        juce::dsp::LinkwitzRileyFilter<float> lowLP, highHP;
        void reset() { lowLP.reset(); highHP.reset(); }
    };
    std::vector<ChannelSplit> splits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEnhancerAudioProcessor)
};
