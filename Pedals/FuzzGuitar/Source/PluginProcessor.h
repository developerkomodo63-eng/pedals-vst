#pragma once

#include <JuceHeader.h>

class FuzzGuitarAudioProcessor : public juce::AudioProcessor
{
public:
    FuzzGuitarAudioProcessor();
    ~FuzzGuitarAudioProcessor() override;

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
    juce::dsp::StateVariableTPTFilter<float> hpFilter;
    juce::dsp::StateVariableTPTFilter<float> lpFilter;

    // el fuzz genera muchos mas armonicos que una distorsion tipo drive,
    // asi que el oversampling importa mas aca para controlar el aliasing
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::vector<float> dcBlockerX1, dcBlockerY1;
    static constexpr float dcBlockerR = 0.995f;
    juce::AudioBuffer<float> dryBuffer;

    float processFuzzSample (float x, float hardness, float bias) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzGuitarAudioProcessor)
};
