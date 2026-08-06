#pragma once

#include <JuceHeader.h>

class TransientShaperAudioProcessor : public juce::AudioProcessor
{
public:
    TransientShaperAudioProcessor();
    ~TransientShaperAudioProcessor() override;

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
    // dos envolventes a distinta velocidad: la diferencia entre la rapida
    // (sigue el golpe) y la lenta (sigue el cuerpo/sustain) funciona como
    // detector de transitorio. Sin filtros, sin FFT -- exactamente la
    // tecnica clasica de los "transient designer" analogicos.
    float fastEnvelope = 0.0f, slowEnvelope = 0.0f;
    float fastAttackCoeff = 0.0f, fastReleaseCoeff = 0.0f;
    float slowAttackCoeff = 0.0f, slowReleaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientShaperAudioProcessor)
};
