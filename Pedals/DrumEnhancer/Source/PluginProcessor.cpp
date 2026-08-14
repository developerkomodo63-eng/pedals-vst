#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout DrumEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CROSSOVER", 1 }, "Crossover",
        juce::NormalisableRange<float> { 200.0f, 2000.0f, 0.0f, 0.4f }, 600.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LOWDRIVE", 1 }, "Low Process", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "HIGHDRIVE", 1 }, "High Process", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

DrumEnhancerAudioProcessor::DrumEnhancerAudioProcessor()
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

DrumEnhancerAudioProcessor::~DrumEnhancerAudioProcessor()
{
}

void DrumEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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
}

void DrumEnhancerAudioProcessor::releaseResources()
{
}

bool DrumEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DrumEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float crossover = apvts.getRawParameterValue ("CROSSOVER")->load();
    const float lowDrive  = apvts.getRawParameterValue ("LOWDRIVE")->load();
    const float highDrive = apvts.getRawParameterValue ("HIGHDRIVE")->load();
    const float mix       = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb   = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    for (auto& s : splits)
    {
        s.lowLP.setCutoffFrequency (crossover);
        s.highHP.setCutoffFrequency (crossover);
    }

    // cuanto mas alto el drive, mas ganancia interna antes de saturar
    // (rango pensado para armonicos sutiles, no para distorsion audible)
    const float lowGainStage  = 1.0f + lowDrive * 5.0f;
    const float highGainStage = 1.0f + highDrive * 5.0f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& s = splits[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            const float low  = s.lowLP.processSample (0, dry);
            const float high = s.highHP.processSample (0, dry);

            // graves: saturacion suave (2do armonico, mas "punch" percibido)
            const float lowExcited = std::tanh (low * lowGainStage) / juce::jmax (lowGainStage, 1.0f);

            // agudos: mismo tipo de saturacion, ganancia mas moderada para
            // aportar "aire"/definicion sin volverse aspero
            const float highExcited = std::tanh (high * highGainStage) / juce::jmax (highGainStage, 1.0f);

            const float enhanced = lowExcited * lowDrive + highExcited * highDrive
                                  + low * (1.0f - lowDrive) + high * (1.0f - highDrive);

            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* DrumEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void DrumEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DrumEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumEnhancerAudioProcessor();
}
