#pragma once

#include <JuceHeader.h>

class OctaverAudioProcessor : public juce::AudioProcessor
{
public:
    OctaverAudioProcessor();
    ~OctaverAudioProcessor() override;

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
    // tecnica analogica clasica: NO usa deteccion de pitch. Un pasa-bajos
    // limpia la señal para que los cruces por cero sean estables, y un
    // "flip-flop" que se invierte en cada cruce genera una onda cuadrada a
    // mitad de frecuencia (una octava abajo). Dividir de nuevo esa onda
    // dobla el efecto (-2 octavas). Para +1 octava, se usa rectificacion de
    // onda completa (abs), que naturalmente contiene el doble de frecuencia.
    juce::dsp::StateVariableTPTFilter<float> trackingFilter;

    bool sub1State = false;
    bool sub2State = false;
    int crossingCounter = 0;
    float lastFilteredSample = 0.0f;

    float envelopeState = 0.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;

    float upDcX1 = 0.0f, upDcY1 = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctaverAudioProcessor)
};
