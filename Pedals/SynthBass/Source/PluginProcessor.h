#pragma once

#include <JuceHeader.h>
#include "PitchTracker.h"
#include "Oscillator.h"

class SynthBassAudioProcessor : public juce::AudioProcessor
{
public:
    SynthBassAudioProcessor();
    ~SynthBassAudioProcessor() override;

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
    PitchTracker pitchTracker;
    BlepOscillator oscillator;

    // suavizado de frecuencia (glide/portamento) hecho a mano: un solo polo,
    // asi cambiar el tiempo de glide en caliente no "salta" el valor actual
    float smoothedFreqValue = 220.0f;

    // seguidor de envolvente simple (un polo, ataque/release distintos)
    // que abre y cierra el volumen del synth segun la señal de entrada
    float envelopeState = 0.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;

    float currentTargetFrequency = 220.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthBassAudioProcessor)
};
