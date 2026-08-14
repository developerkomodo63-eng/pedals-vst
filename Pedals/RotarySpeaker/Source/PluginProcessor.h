#pragma once

#include <JuceHeader.h>

class RotarySpeakerAudioProcessor : public juce::AudioProcessor
{
public:
    RotarySpeakerAudioProcessor();
    ~RotarySpeakerAudioProcessor() override;

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
    // simplificacion honesta: un bafle rotativo real tiene un rotor de agudos
    // (horn) y uno de graves (drum) girando a velocidades ligeramente
    // distintas; ac[a] usamos un solo rotor combinado (AM + modulacion de
    // pitch desde la misma fase, con 90 grados de diferencia como en la
    // fisica real del Doppler de una fuente girando) para mantenerlo
    // liviano. La velocidad no cambia de golpe entre lento/rapido: se
    // "acelera" y "frena" con inercia, como el motor real.
    static constexpr double maxLineMs = 8.0;
    std::vector<std::vector<float>> lines;
    std::vector<int> writePos;

    double sampleRate = 44100.0;
    float rotorPhase = 0.0f;
    float currentRateHz = 0.8f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RotarySpeakerAudioProcessor)
};
