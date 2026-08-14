#pragma once

#include <JuceHeader.h>

class BroadcastCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    BroadcastCompressorAudioProcessor();
    ~BroadcastCompressorAudioProcessor() override;

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
    // filosofia "un solo knob" de compresores simples de radio/vivo: un
    // solo control de cantidad (mapea threshold+ratio juntos),
    // tiempos fijos tipo opto (ataque no tan rapido, release largo) y
    // rodilla suave -- pensado para ser "a prueba de balas", no para
    // ajustar cada parametro a mano como el Compressor normal.
    float envelopeDb = -100.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BroadcastCompressorAudioProcessor)
};
