#pragma once

#include <JuceHeader.h>

class GlitchMachineAudioProcessor : public juce::AudioProcessor
{
public:
    GlitchMachineAudioProcessor();
    ~GlitchMachineAudioProcessor() override;

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
    // captura continua en un buffer circular (siempre grabando, viva la
    // señal que sea), dividida en "slots" de tamaño Rate. Al empezar cada
    // slot se sortea, pesado por Chaos, si ese slot va a ser: paso normal,
    // un tartamudeo (loopea un fragmento recien capturado, a veces al
    // reves, a veces con salto de pitch cuantizado), o silencio. Lectura
    // sin interpolar a proposito -- un poco de aspereza es parte del
    // caracter de un glitch, no hace falta gastar CPU suavizandola.
    enum class SlotMode { Passthrough, Glitch, Silence };

    struct ChannelState
    {
        std::vector<float> captureBuffer;
        int writePos = 0;

        int slotSamplesRemaining = 0;
        SlotMode currentMode = SlotMode::Passthrough;

        int segmentStart = 0;
        int segmentLength = 1;
        float direction = 1.0f;
        float pitchMult = 1.0f;
        float progress = 0.0f;
    };
    std::vector<ChannelState> channels;

    juce::Random rng;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchMachineAudioProcessor)
};
