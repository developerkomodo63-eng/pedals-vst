#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout NoiseGateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "THRESHOLD", 1 }, "Threshold", -80.0f, -20.0f, -50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ATTACK", 1 }, "Attack",
        juce::NormalisableRange<float> { 0.1f, 50.0f, 0.0f, 0.4f }, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RELEASE", 1 }, "Release",
        juce::NormalisableRange<float> { 10.0f, 500.0f, 0.0f, 0.4f }, 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RANGE", 1 }, "Range", -80.0f, 0.0f, -60.0f));

    return { params.begin(), params.end() };
}

NoiseGateAudioProcessor::NoiseGateAudioProcessor()
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

NoiseGateAudioProcessor::~NoiseGateAudioProcessor()
{
}

void NoiseGateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    envelopeDb = -100.0f;
    attackCoeff  = std::exp (-1.0f / (0.001f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.100f * (float) sampleRate));
}

void NoiseGateAudioProcessor::releaseResources()
{
}

bool NoiseGateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void NoiseGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float thresholdDb = apvts.getRawParameterValue ("THRESHOLD")->load();
    const float attackMs    = apvts.getRawParameterValue ("ATTACK")->load();
    const float releaseMs   = apvts.getRawParameterValue ("RELEASE")->load();
    const float rangeDb     = apvts.getRawParameterValue ("RANGE")->load();

    const float sr = (float) getSampleRate();
    attackCoeff  = std::exp (-1.0f / (juce::jmax (attackMs, 0.1f)  / 1000.0f * sr));
    releaseCoeff = std::exp (-1.0f / (juce::jmax (releaseMs, 1.0f) / 1000.0f * sr));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float peak = std::abs (buffer.getReadPointer (0)[sample]);
        const float peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);

        const float envCoeff = (peakDb > envelopeDb) ? attackCoeff : releaseCoeff;
        envelopeDb = peakDb + envCoeff * (envelopeDb - peakDb);

        // knee de 12dB alrededor del umbral para que no cierre de golpe
        const float gateGainDb = juce::jlimit (rangeDb, 0.0f,
            juce::jmap (envelopeDb, thresholdDb - 12.0f, thresholdDb, rangeDb, 0.0f));

        const float gateGain = juce::Decibels::decibelsToGain (gateGainDb);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            channelData[sample] *= gateGain;
        }
    }
}

juce::AudioProcessorEditor* NoiseGateAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void NoiseGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NoiseGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseGateAudioProcessor();
}
