#pragma once

#include <JuceHeader.h>

class ConvolutionReverbAudioProcessor : public juce::AudioProcessor
{
public:
    ConvolutionReverbAudioProcessor();
    ~ConvolutionReverbAudioProcessor() override;

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
    // el motor de convolucion agrega su propia latencia de procesamiento
    // por bloques (FFT particionada); reportamos una cola generosa porque
    // los IRs de reverbs de sala/plate pueden durar varios segundos
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

    // llamado desde el boton "Cargar IR..." del editor
    void loadImpulseResponseFile (const juce::File& file);
    juce::String getLoadedFileName() const { return loadedFileName; }

private:
    // JUCE trae su propio motor de convolucion particionada por FFT --
    // no hace falta escribir eso a mano. Es mas pesado que el resto del
    // pack (usa FFT internamente), pero sigue siendo una tecnica estandar,
    // no un modelo entrenado.
    juce::dsp::Convolution convolution;

    juce::AudioBuffer<float> dryBuffer;

    juce::String loadedFileName;
    juce::String lastLoadedFullPath;
    juce::File pendingLoadOnRestore; // path guardado en el estado, se carga en prepareToPlay
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionReverbAudioProcessor)
};
