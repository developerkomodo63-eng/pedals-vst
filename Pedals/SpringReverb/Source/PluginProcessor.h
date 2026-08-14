#pragma once

#include <JuceHeader.h>
#include <array>

class SpringReverbAudioProcessor : public juce::AudioProcessor
{
public:
    SpringReverbAudioProcessor();
    ~SpringReverbAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 3.0; }

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
    // spring tank algoritmico: unos pocos comb filters (delay + feedback)
    // en paralelo con tiempos no relacionados armonicamente (para que no
    // suene a flanger), sumados y pasados por un par de allpass en serie
    // para difusion, banda limitada (una cinta/resorte real no reproduce
    // ni graves ni agudos extremos), con un LFO chico moviendo los delays
    // de los combs -- eso es lo que da el caracteristico "boing" metalico.
    static constexpr int numCombs = 3;
    static constexpr std::array<float, numCombs> combTimesMs { 29.0f, 37.0f, 43.0f };

    struct ChannelState
    {
        std::array<std::vector<float>, numCombs> combLines;
        std::array<int, numCombs> combWritePos {};

        std::vector<float> allpass1Line, allpass2Line;
        int allpass1WritePos = 0, allpass2WritePos = 0;

        juce::dsp::IIR::Filter<float> hpFilter, lpFilter;

        void reset()
        {
            allpass1WritePos = 0;
            allpass2WritePos = 0;
            combWritePos.fill (0);
            hpFilter.reset();
            lpFilter.reset();
        }
    };
    std::vector<ChannelState> channels;

    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpringReverbAudioProcessor)
};
