#pragma once

#include <JuceHeader.h>

class DistortionGuitarAudioProcessor : public juce::AudioProcessor
{
public:
    DistortionGuitarAudioProcessor();
    ~DistortionGuitarAudioProcessor() override;

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

    // scoop de medios: un filtro IIR *por canal* (compartir una sola
    // instancia entre canales corrompe el estado interno z1/z2, mezclando
    // el historial de L y R)
    std::vector<juce::dsp::IIR::Filter<float>> scoopFilters;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::vector<float> dcBlockerX1, dcBlockerY1;
    static constexpr float dcBlockerR = 0.995f;
    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate = 44100.0;

    static float processDistortionSample (float x, float bias) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionGuitarAudioProcessor)
};
