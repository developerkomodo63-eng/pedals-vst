#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout MonoMakerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FREQUENCY", 1 }, "Frequency",
        juce::NormalisableRange<float> { 40.0f, 500.0f, 0.0f, 0.4f }, 120.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WIDTH", 1 }, "Width Above", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -6.0f, 6.0f, 0.0f));

    return { params.begin(), params.end() };
}

MonoMakerAudioProcessor::MonoMakerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
}

MonoMakerAudioProcessor::~MonoMakerAudioProcessor()
{
}

void MonoMakerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    splits.resize (2);
    for (auto& s : splits)
    {
        s.lowLP.prepare (spec);
        s.highHP.prepare (spec);
        s.lowLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        s.highHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        s.reset();
    }
}

void MonoMakerAudioProcessor::releaseResources()
{
}

bool MonoMakerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // este pedal solo tiene sentido en estereo (necesita dos canales para
    // poder sumarlos), asi que exigimos estereo estricto en vez del
    // mono-o-estereo flexible del resto del pack
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void MonoMakerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    if (getTotalNumInputChannels() < 2 || getTotalNumOutputChannels() < 2)
        return;

    const int numSamples = buffer.getNumSamples();

    const float freq   = apvts.getRawParameterValue ("FREQUENCY")->load();
    const float width  = apvts.getRawParameterValue ("WIDTH")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    for (auto& s : splits)
    {
        s.lowLP.setCutoffFrequency (freq);
        s.highHP.setCutoffFrequency (freq);
    }

    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float lowL  = splits[0].lowLP.processSample (0, left[sample]);
        const float highL = splits[0].highHP.processSample (0, left[sample]);
        const float lowR  = splits[1].lowLP.processSample (0, right[sample]);
        const float highR = splits[1].highHP.processSample (0, right[sample]);

        const float monoLow = (lowL + lowR) * 0.5f;

        // "Width Above" en 0 deja tambien los agudos centrados (mono
        // total); en 1, el estereo de arriba del cruce queda intacto
        const float mid = (highL + highR) * 0.5f;
        const float sideL = highL - mid;
        const float sideR = highR - mid;

        left[sample]  = (monoLow + mid + sideL * width) * outputGain;
        right[sample] = (monoLow + mid + sideR * width) * outputGain;
    }
}

juce::AudioProcessorEditor* MonoMakerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void MonoMakerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MonoMakerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonoMakerAudioProcessor();
}
