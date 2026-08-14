#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout BoostAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GAIN", 1 }, "Gain", -6.0f, 24.0f, 6.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BASS", 1 }, "Bass Contour", -6.0f, 6.0f, 0.0f));

    return { params.begin(), params.end() };
}

BoostAudioProcessor::BoostAudioProcessor()
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

BoostAudioProcessor::~BoostAudioProcessor()
{
}

void BoostAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    bassShelfFilters.resize ((size_t) numChannels);
    for (auto& f : bassShelfFilters)
        f.reset();
}

void BoostAudioProcessor::releaseResources()
{
}

bool BoostAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void BoostAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float gainDb = apvts.getRawParameterValue ("GAIN")->load();
    const float bassDb = apvts.getRawParameterValue ("BASS")->load();
    const float gain = juce::Decibels::decibelsToGain (gainDb);

    auto shelfCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (
        currentSampleRate, 120.0f, 0.7f, juce::Decibels::decibelsToGain (bassDb));
    for (auto& f : bassShelfFilters)
        *f.coefficients = shelfCoeffs;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& shelf = bassShelfFilters[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float shaped = shelf.processSample (channelData[sample]);
            channelData[sample] = shaped * gain;
        }
    }
}

juce::AudioProcessorEditor* BoostAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void BoostAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BoostAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BoostAudioProcessor();
}
