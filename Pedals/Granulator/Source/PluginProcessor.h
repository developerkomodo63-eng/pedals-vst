#pragma once

#include <JuceHeader.h>
#include <array>

class GranulatorAudioProcessor : public juce::AudioProcessor
{
public:
    GranulatorAudioProcessor();
    ~GranulatorAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 2.0; }

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
    // sintesis granular clasica: buffer de captura continua, y un puñado
    // de "granos" (ventana Hann + lectura interpolada) que se disparan a
    // un ritmo controlado por Density, cada uno con su propia posicion de
    // lectura (jitter) y velocidad (pitch spread), superpuestos entre si
    // para formar una textura en vez de una sola nota. Freeze corta la
    // captura para loopear un instante congelado indefinidamente.
    static constexpr int numGrains = 6;

    struct Grain
    {
        float progress = 1.0f; // >=1 significa inactivo
        float readPos = 0.0f;
        float pitchMult = 1.0f;
    };

    struct ChannelState
    {
        std::vector<float> captureBuffer;
        int writePos = 0;
        std::array<Grain, numGrains> grains;
        int nextGrainSlot = 0;
        int triggerCountdown = 0;
    };
    std::vector<ChannelState> channels;

    juce::Random rng;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulatorAudioProcessor)
};
