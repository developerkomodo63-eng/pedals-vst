#pragma once

#include <JuceHeader.h>

class PreampAudioProcessor : public juce::AudioProcessor
{
public:
    PreampAudioProcessor();
    ~PreampAudioProcessor() override;

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
    // dos shelfs (graves/agudos) *por canal*, saturacion suave sin
    // oversampling (a diferencia del Overdrive/Fuzz/Distortion: la idea del
    // preamp es dar calidez sutil, no gancho armonico fuerte, asi que no
    // hace falta pagar el costo extra del oversampler)
    struct ChannelTone
    {
        juce::dsp::IIR::Filter<float> bass, treble;
        void reset() { bass.reset(); treble.reset(); }
    };
    std::vector<ChannelTone> tones;

    juce::AudioBuffer<float> dryBuffer;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreampAudioProcessor)
};
