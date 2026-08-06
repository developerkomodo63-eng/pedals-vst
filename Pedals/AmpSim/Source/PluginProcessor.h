#pragma once

#include <JuceHeader.h>

class AmpSimAudioProcessor : public juce::AudioProcessor
{
public:
    AmpSimAudioProcessor();
    ~AmpSimAudioProcessor() override;

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
    // Esto NO es un modelador neuronal (eso implicaria una red entrenada,
    // ordenes de magnitud mas CPU). Es la tecnica "clasica" de ampsim:
    // dos etapas de saturacion en cascada (preamp + power amp) con
    // caracter distinto cada una, un tone stack de 3 bandas, y un filtro
    // que aproxima la respuesta de un gabinete (en vez de convolucion
    // real con una IR, que es mucho mas pesada).
    juce::dsp::StateVariableTPTFilter<float> hpFilter;

    struct ChannelToneStack
    {
        juce::dsp::IIR::Filter<float> bass, mid, treble;
        juce::dsp::IIR::Filter<float> cabHighCut, cabLowCut, cabPresence;
        void reset()
        {
            bass.reset(); mid.reset(); treble.reset();
            cabHighCut.reset(); cabLowCut.reset(); cabPresence.reset();
        }
    };
    std::vector<ChannelToneStack> channels;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::vector<float> dcBlockerX1, dcBlockerY1;
    static constexpr float dcBlockerR = 0.995f;

    double currentSampleRate = 44100.0;

    static float preampStage (float x) noexcept;
    static float powerAmpStage (float x) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpSimAudioProcessor)
};
