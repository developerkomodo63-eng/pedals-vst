#pragma once

#include <JuceHeader.h>
#include <array>

class MultibandCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    MultibandCompressorAudioProcessor();
    ~MultibandCompressorAudioProcessor() override;

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
    // Cruces Linkwitz-Riley: LP+HP con la misma frecuencia y orden suman de
    // vuelta a la señal original sin colorear el sonido (a diferencia de
    // filtros IIR comunes, que sumados generan comb filtering). Para 3
    // bandas hacen falta 2 puntos de cruce, cada uno con su filtro *por canal*.
    struct ChannelCrossovers
    {
        juce::dsp::LinkwitzRileyFilter<float> lowSplitLP, lowSplitHP;
        juce::dsp::LinkwitzRileyFilter<float> highSplitLP, highSplitHP;
        void reset() { lowSplitLP.reset(); lowSplitHP.reset(); highSplitLP.reset(); highSplitHP.reset(); }
    };
    std::vector<ChannelCrossovers> crossovers;

    // envolvente de compresion independiente por banda (compartida entre
    // canales, como el resto de los pedales de esta serie)
    std::array<float, 3> bandEnvelopeDb { -100.0f, -100.0f, -100.0f };
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandCompressorAudioProcessor)
};
