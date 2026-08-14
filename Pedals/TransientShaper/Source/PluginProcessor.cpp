#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout TransientShaperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ATTACK", 1 }, "Attack", -15.0f, 15.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUSTAIN", 1 }, "Sustain", -15.0f, 15.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SENSITIVITY", 1 }, "Sensitivity", 0.5f, 5.0f, 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

TransientShaperAudioProcessor::TransientShaperAudioProcessor()
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

TransientShaperAudioProcessor::~TransientShaperAudioProcessor()
{
}

void TransientShaperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    fastEnvelope = slowEnvelope = 0.0f;

    fastAttackCoeff  = std::exp (-1.0f / (0.0005f * (float) sampleRate));
    fastReleaseCoeff = std::exp (-1.0f / (0.050f  * (float) sampleRate));
    slowAttackCoeff  = std::exp (-1.0f / (0.030f  * (float) sampleRate));
    slowReleaseCoeff = std::exp (-1.0f / (0.300f  * (float) sampleRate));
}

void TransientShaperAudioProcessor::releaseResources()
{
}

bool TransientShaperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void TransientShaperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float attackDb   = apvts.getRawParameterValue ("ATTACK")->load();
    const float sustainDb  = apvts.getRawParameterValue ("SUSTAIN")->load();
    const float sensitivity= apvts.getRawParameterValue ("SENSITIVITY")->load();
    const float mix        = apvts.getRawParameterValue ("MIX")->load();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float peak = std::abs (buffer.getReadPointer (0)[sample]);

        const float fastCoeff = (peak > fastEnvelope) ? fastAttackCoeff : fastReleaseCoeff;
        fastEnvelope = peak + fastCoeff * (fastEnvelope - peak);

        const float slowCoeff = (peak > slowEnvelope) ? slowAttackCoeff : slowReleaseCoeff;
        slowEnvelope = peak + slowCoeff * (slowEnvelope - peak);

        // diferencia entre envolventes: positiva justo cuando pega un
        // transitorio (la rapida todavia no la alcanzo la lenta)
        const float detector = std::tanh ((fastEnvelope - slowEnvelope) * sensitivity * 20.0f);
        const float attackWeight = 0.5f * (1.0f + detector); // 1 = transitorio, 0 = sustain

        const float gainDb = attackWeight * attackDb + (1.0f - attackWeight) * sustainDb;
        const float gain = juce::Decibels::decibelsToGain (gainDb);
        const float blendedGain = (1.0f - mix) + mix * gain;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            channelData[sample] *= blendedGain;
        }
    }
}

juce::AudioProcessorEditor* TransientShaperAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void TransientShaperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TransientShaperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TransientShaperAudioProcessor();
}
