#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout BitcrusherAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "BITS", 1 }, "Bit Depth", 2, 16, 8));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "RATEDIV", 1 }, "Rate Reduction", 1, 50, 4));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

BitcrusherAudioProcessor::BitcrusherAudioProcessor()
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

BitcrusherAudioProcessor::~BitcrusherAudioProcessor()
{
}

void BitcrusherAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    heldSample.assign ((size_t) numChannels, 0.0f);
    holdCounter.assign ((size_t) numChannels, 0);
}

void BitcrusherAudioProcessor::releaseResources()
{
}

bool BitcrusherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void BitcrusherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const int bits      = (int) apvts.getRawParameterValue ("BITS")->load();
    const int rateDiv   = (int) apvts.getRawParameterValue ("RATEDIV")->load();
    const float mix     = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    // niveles de cuantizacion: 2^bits pasos repartidos en el rango -1..1
    const float levels = (float) (1 << bits);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        float& held = heldSample[(size_t) channel];
        int& counter = holdCounter[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            // sample & hold: solo "muestreamos" de nuevo cada rateDiv
            // muestras, el resto del tiempo repetimos el ultimo valor
            // (asi se simula una tasa de muestreo mas baja sin resamplear)
            if (counter <= 0)
            {
                held = dry;
                counter = rateDiv;
            }
            --counter;

            // cuantizacion de amplitud (reduccion de bit depth)
            const float crushed = std::floor (held * levels) / levels;

            channelData[sample] = (dry * (1.0f - mix) + crushed * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* BitcrusherAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void BitcrusherAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BitcrusherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BitcrusherAudioProcessor();
}
