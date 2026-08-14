#include "PluginProcessor.h"
#include "DevKomodoUI.h"
#include <array>

juce::AudioProcessorValueTreeState::ParameterLayout DeesserAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FREQUENCY", 1 }, "Frequency",
        juce::NormalisableRange<float> { 3000.0f, 9000.0f, 0.0f, 0.4f }, 6000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "THRESHOLD", 1 }, "Threshold", -40.0f, 0.0f, -20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATIO", 1 }, "Ratio",
        juce::NormalisableRange<float> { 1.0f, 20.0f, 0.0f, 0.4f }, 6.0f));

    return { params.begin(), params.end() };
}

DeesserAudioProcessor::DeesserAudioProcessor()
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

DeesserAudioProcessor::~DeesserAudioProcessor()
{
}

void DeesserAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    splits.resize ((size_t) numChannels);
    for (auto& s : splits)
    {
        s.lowLP.prepare (spec);
        s.highHP.prepare (spec);
        s.lowLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        s.highHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        s.reset();
    }

    envelopeDb = -100.0f;
    // rapido: la sibilancia es corta, si el attack es lento se cuela igual
    attackCoeff  = std::exp (-1.0f / (0.001f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.060f * (float) sampleRate));
}

void DeesserAudioProcessor::releaseResources()
{
}

bool DeesserAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DeesserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float freq      = apvts.getRawParameterValue ("FREQUENCY")->load();
    const float threshold = apvts.getRawParameterValue ("THRESHOLD")->load();
    const float ratio     = apvts.getRawParameterValue ("RATIO")->load();

    for (auto& s : splits)
    {
        s.lowLP.setCutoffFrequency (freq);
        s.highHP.setCutoffFrequency (freq);
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // separamos canal 0 primero para medir el nivel de sibilancia y
        // linkear la reduccion a todos los canales (como el resto de la serie)
        float detectHigh = 0.0f;

        std::array<float, 2> lowBand { 0.0f, 0.0f };
        std::array<float, 2> highBand { 0.0f, 0.0f };

        for (int channel = 0; channel < totalNumInputChannels && channel < 2; ++channel)
        {
            const float in = buffer.getReadPointer (channel)[sample];
            auto& s = splits[(size_t) channel];
            lowBand[(size_t) channel]  = s.lowLP.processSample (0, in);
            highBand[(size_t) channel] = s.highHP.processSample (0, in);
        }

        detectHigh = std::abs (highBand[0]);
        const float peakDb = juce::Decibels::gainToDecibels (detectHigh, -100.0f);
        const float envCoeff = (peakDb > envelopeDb) ? attackCoeff : releaseCoeff;
        envelopeDb = peakDb + envCoeff * (envelopeDb - peakDb);

        float gainReductionDb = 0.0f;
        if (envelopeDb > threshold)
        {
            const float overDb = envelopeDb - threshold;
            gainReductionDb = overDb - overDb / ratio;
        }
        const float highGain = juce::Decibels::decibelsToGain (-gainReductionDb);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            const int srcChannel = juce::jmin (channel, 1);
            float* channelData = buffer.getWritePointer (channel);
            channelData[sample] = lowBand[(size_t) srcChannel] + highBand[(size_t) srcChannel] * highGain;
        }
    }
}

juce::AudioProcessorEditor* DeesserAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void DeesserAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DeesserAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeesserAudioProcessor();
}
