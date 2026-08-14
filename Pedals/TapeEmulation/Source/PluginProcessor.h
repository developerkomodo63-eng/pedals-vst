#pragma once

#include <JuceHeader.h>

class TapeEmulationAudioProcessor : public juce::AudioProcessor
{
public:
    TapeEmulationAudioProcessor();
    ~TapeEmulationAudioProcessor() override;

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
    // wow (LFO lento, ~0.7Hz) + flutter (LFO rapido, ~8Hz) suman su
    // modulacion sobre una linea de delay corta -> variacion de pitch,
    // igual que la inestabilidad mecanica real de una cinta. Mismo truco
    // que el Doubler/Chorus, solo que ac[a] el objetivo es "imperfeccion",
    // no ancho estereo.
    static constexpr double maxLineMs = 8.0;
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    juce::dsp::IIR::Filter<float> hfRolloffL, hfRolloffR;

    juce::Random noiseGenL, noiseGenR;

    double sampleRate = 44100.0;
    float wowPhase = 0.0f, flutterPhase = 0.0f;

    static float tapeSaturate (float x, float amount) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeEmulationAudioProcessor)
};
