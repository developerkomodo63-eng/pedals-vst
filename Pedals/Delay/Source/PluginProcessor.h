#pragma once

#include <JuceHeader.h>

class DelayAudioProcessor : public juce::AudioProcessor
{
public:
    DelayAudioProcessor();
    ~DelayAudioProcessor() override;

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
    // Con Feedback alto (0.95) y Time al maximo (2000ms), la cola real tarda
    // muchisimo mas que unos pocos segundos en apagarse del todo. Reportamos
    // un valor generoso para que el host no corte el eco de golpe al hacer
    // bypass o parar la reproduccion; no es "infinito" pero cubre el uso normal.
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
    static constexpr double maxDelaySeconds = 2.0;

    // buffer circular por canal, reservado una sola vez en prepareToPlay
    std::vector<std::vector<float>> delayLines;
    std::vector<int> writePos;

    // filtro pasa-bajos de una sola muestra en el camino del feedback
    // (asi las repeticiones se van oscureciendo, como un delay analogico)
    std::vector<float> feedbackFilterState;

    double sampleRate = 44100.0;

    // ducking: el eco se agacha mientras la señal seca esta sonando, y se
    // abre solo en los silencios -- asi el delay nunca tapa la frase. Sin
    // sidechain externo: se auto-detecta contra su propia entrada.
    float duckEnvelope = 0.0f;
    float duckAttackCoeff = 0.0f, duckReleaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessor)
};
