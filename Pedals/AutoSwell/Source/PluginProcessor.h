#pragma once

#include <JuceHeader.h>

class AutoSwellAudioProcessor : public juce::AudioProcessor
{
public:
    AutoSwellAudioProcessor();
    ~AutoSwellAudioProcessor() override;

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
    // detecta el ataque de cada nota nueva (envolvente que sube rapido) y
    // en vez de dejarlo pasar, arranca una rampa de volumen desde 0 hasta
    // el nivel de la nota, tan lenta como el knob de Swell diga -- asi
    // cada pulsacion suena como si alguien moviera un pedal de volumen a
    // mano, tipo violin/pedal steel
    float envelopeState = 0.0f;
    float envAttackCoeff = 0.0f, envReleaseCoeff = 0.0f;

    float swellGain = 0.0f;
    float lastEnvelope = 0.0f;

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoSwellAudioProcessor)
};
