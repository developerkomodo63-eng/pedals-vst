#pragma once

#include <JuceHeader.h>
#include <array>

class PhaserAudioProcessor : public juce::AudioProcessor
{
public:
    PhaserAudioProcessor();
    ~PhaserAudioProcessor() override;

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
    static constexpr int maxStages = 8;
    static constexpr int maxChannels = 2;

    // cada etapa allpass de 1er orden necesita 2 estados (x1, y1) por canal
    struct AllpassState { float x1 = 0.0f, y1 = 0.0f; };
    std::array<std::array<AllpassState, maxStages>, maxChannels> stages;

    std::array<float, maxChannels> feedbackState { 0.0f, 0.0f };

    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;

    static float processAllpass (AllpassState& state, float x, float a) noexcept
    {
        const float y = a * x + state.x1 - a * state.y1;
        state.x1 = x;
        state.y1 = y;
        return y;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaserAudioProcessor)
};
