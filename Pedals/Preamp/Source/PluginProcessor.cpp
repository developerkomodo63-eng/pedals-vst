#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout PreampAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 }, "Drive", 1.0f, 8.0f, 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BASS", 1 }, "Bass", -6.0f, 6.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TREBLE", 1 }, "Treble", -6.0f, 6.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BLEND", 1 }, "Blend", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

PreampAudioProcessor::PreampAudioProcessor()
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

PreampAudioProcessor::~PreampAudioProcessor()
{
}

void PreampAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    tones.resize ((size_t) numChannels);
    for (auto& t : tones)
        t.reset();

    dryBuffer.setSize (numChannels, samplesPerBlock);
}

void PreampAudioProcessor::releaseResources()
{
}

bool PreampAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void PreampAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float drive   = apvts.getRawParameterValue ("DRIVE")->load();
    const float bassDb  = apvts.getRawParameterValue ("BASS")->load();
    const float trebleDb = apvts.getRawParameterValue ("TREBLE")->load();
    const float blend   = apvts.getRawParameterValue ("BLEND")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    auto bassCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (
        currentSampleRate, 100.0f, 0.7f, juce::Decibels::decibelsToGain (bassDb));
    auto trebleCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
        currentSampleRate, 3500.0f, 0.7f, juce::Decibels::decibelsToGain (trebleDb));

    for (auto& t : tones)
    {
        *t.bass.coefficients   = bassCoeffs;
        *t.treble.coefficients = trebleCoeffs;
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        const float* dry = dryBuffer.getReadPointer (channel);
        auto& t = tones[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // saturacion suave: nada de oversampling, el objetivo es
            // calidez sutil, no un gancho armonico fuerte como en el
            // Overdrive/Distortion/Fuzz
            const float driven = channelData[sample] * drive;
            const float saturated = std::tanh (driven) / std::tanh (juce::jmax (drive, 1.0f));

            float shaped = t.bass.processSample (saturated);
            shaped = t.treble.processSample (shaped);

            channelData[sample] = (dry[sample] * (1.0f - blend) + shaped * blend) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* PreampAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void PreampAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PreampAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PreampAudioProcessor();
}
