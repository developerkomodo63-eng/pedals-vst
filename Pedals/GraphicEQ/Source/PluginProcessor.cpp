#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout GraphicEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (int i = 0; i < (int) bandFrequencies.size(); ++i)
    {
        const float freq = bandFrequencies[(size_t) i];
        juce::String label = freq >= 1000.0f
            ? juce::String (freq / 1000.0f, 1) + "k"
            : juce::String ((int) freq);

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "BAND" + juce::String (i), 1 },
            label + " Hz", -12.0f, 12.0f, 0.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

GraphicEQAudioProcessor::GraphicEQAudioProcessor()
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

GraphicEQAudioProcessor::~GraphicEQAudioProcessor()
{
}

void GraphicEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    bands.resize ((size_t) numChannels);
    for (auto& b : bands)
        b.reset();
}

void GraphicEQAudioProcessor::releaseResources()
{
}

bool GraphicEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GraphicEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    std::array<std::array<float, 6>, numBands> coeffs;
    static constexpr const char* bandIds[numBands] =
        { "BAND0", "BAND1", "BAND2", "BAND3", "BAND4", "BAND5", "BAND6", "BAND7" };

    for (int i = 0; i < numBands; ++i)
    {
        const float gainDb = apvts.getRawParameterValue (bandIds[i])->load();
        // Q moderado (~1.4) para que bandas vecinas se superpongan un poco,
        // como en un EQ grafico real, en vez de sonar como muescas aisladas
        coeffs[(size_t) i] = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
            currentSampleRate, bandFrequencies[(size_t) i], 1.4f,
            juce::Decibels::decibelsToGain (gainDb));
    }

    for (auto& b : bands)
        for (int i = 0; i < numBands; ++i)
            *b.filters[(size_t) i].coefficients = coeffs[(size_t) i];

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& b = bands[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float y = channelData[sample];
            for (int i = 0; i < numBands; ++i)
                y = b.filters[(size_t) i].processSample (y);
            channelData[sample] = y * outputGain;
        }
    }
}

juce::AudioProcessorEditor* GraphicEQAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void GraphicEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GraphicEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GraphicEQAudioProcessor();
}
