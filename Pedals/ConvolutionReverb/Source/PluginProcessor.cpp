#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ConvolutionReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

ConvolutionReverbAudioProcessor::ConvolutionReverbAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ConvolutionReverbAudioProcessor::~ConvolutionReverbAudioProcessor()
{
}

void ConvolutionReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    convolution.prepare (spec);

    dryBuffer.setSize ((int) spec.numChannels, samplesPerBlock);

    // si el estado guardado traia una ruta de IR (proyecto recargado), la
    // cargamos aca, ya con el sample rate real preparado
    if (pendingLoadOnRestore != juce::File())
    {
        loadImpulseResponseFile (pendingLoadOnRestore);
        pendingLoadOnRestore = juce::File();
    }
}

void ConvolutionReverbAudioProcessor::releaseResources()
{
}

bool ConvolutionReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void ConvolutionReverbAudioProcessor::loadImpulseResponseFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    // limitamos el largo del IR a 10 segundos: un IR mas largo que eso
    // dispara mucho el uso de memoria/CPU de la convolucion sin aportar
    // nada util para un pedal de reverb
    const size_t maxSamples = (size_t) (10.0 * currentSampleRate);

    convolution.loadImpulseResponse (file,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes,
        maxSamples,
        juce::dsp::Convolution::Normalise::yes);

    loadedFileName = file.getFileName();
    lastLoadedFullPath = file.getFullPathName();
}

void ConvolutionReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float mix      = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb  = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    convolution.process (context);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* wet = buffer.getWritePointer (channel);
        const float* dry = dryBuffer.getReadPointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            wet[sample] = (dry[sample] * (1.0f - mix) + wet[sample] * mix) * outputGain;
    }
}

juce::AudioProcessorEditor* ConvolutionReverbAudioProcessor::createEditor()
{
    return new ConvolutionReverbAudioProcessorEditor (*this);
}

void ConvolutionReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    // guardamos la ruta del archivo de IR como un atributo extra en el
    // mismo arbol de estado, asi se recupera al recargar el proyecto
    state.setProperty ("irFilePath", loadedFileName.isEmpty() ? juce::String() : lastLoadedFullPath, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ConvolutionReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml (*xmlState);
            apvts.replaceState (newState);

            const auto path = newState.getProperty ("irFilePath", juce::String()).toString();
            if (path.isNotEmpty())
                pendingLoadOnRestore = juce::File (path);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConvolutionReverbAudioProcessor();
}
