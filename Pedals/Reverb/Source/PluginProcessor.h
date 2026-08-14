#pragma once

#include <JuceHeader.h>
#include <array>

class ReverbAudioProcessor : public juce::AudioProcessor
{
public:
    ReverbAudioProcessor();
    ~ReverbAudioProcessor() override;

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
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> dryBuffer;
    std::vector<juce::dsp::StateVariableTPTFilter<float>> bassWetFilters;

    // Shimmer: la cola de la reverb se pasa por un pitch-shift de +1
    // octava y se reinyecta a la entrada del bloque siguiente -- asi la
    // cola "canta" un armonico arriba en vez de solo apagarse (Freeverb es
    // una caja negra, no podemos meternos en su feedback interno, asi que
    // el loop de shimmer da toda la vuelta *por afuera* del reverb).
    // Esto agrega un bloque de latencia al camino del shimmer (unos
    // pocos ms a tama\u00f1os de buffer tipicos), inaudible para una cola lenta.
    struct OctaveUpTap { float phase = 0.0f, readPos = 0.0f; };
    struct ChannelShimmer
    {
        std::vector<float> pitchBuffer;
        int writePos = 0;
        std::array<OctaveUpTap, 2> taps;
    };
    std::vector<ChannelShimmer> shimmerState;
    juce::AudioBuffer<float> shimmerFeedbackBuffer;

    float processOctaveUp (ChannelShimmer& state, float input, float grainSizeSamples) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbAudioProcessor)
};
