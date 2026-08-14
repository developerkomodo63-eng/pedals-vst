#pragma once

#include <JuceHeader.h>

class TapeDelayAudioProcessor : public juce::AudioProcessor
{
public:
    TapeDelayAudioProcessor();
    ~TapeDelayAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 10.0; }

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
    // fusion del Delay normal con el caracter del Tape: el tiempo de delay
    // se modula con wow+flutter (todas las repeticiones "flotan" un poco,
    // como un transporte de cinta real), y la señal que vuelve al buffer
    // (no la de entrada) pasa por saturacion antes de sumarse -- asi cada
    // repeticion se ensucia un poco mas que la anterior, acumulativo.
    static constexpr double maxDelaySeconds = 2.0;
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    juce::dsp::IIR::Filter<float> toneFilterL, toneFilterR;

    double sampleRate = 44100.0;
    float wowPhase = 0.0f, flutterPhase = 0.0f;

    static float tapeSaturate (float x) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeDelayAudioProcessor)
};
