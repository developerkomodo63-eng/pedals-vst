#pragma once

#include <JuceHeader.h>

class VinylEmulationAudioProcessor : public juce::AudioProcessor
{
public:
    VinylEmulationAudioProcessor();
    ~VinylEmulationAudioProcessor() override;

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
    // primo del Tape: banda limitada como un tocadiscos real (HP+LP), wow
    // mas notorio (motor de plato, no cabezal de cinta), y en vez de hiss
    // continuo, "polvo" -- pulsos aleatorios dispersos (pops/crackle), que
    // es como suena el ruido de un vinilo, no como el siseo de una cinta
    juce::dsp::IIR::Filter<float> hpFilterL, hpFilterR;
    juce::dsp::IIR::Filter<float> lpFilterL, lpFilterR;

    static constexpr double maxLineMs = 6.0;
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    juce::Random crackleRng;
    float crackleEnvelope = 0.0f;

    double sampleRate = 44100.0;
    float wowPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VinylEmulationAudioProcessor)
};
