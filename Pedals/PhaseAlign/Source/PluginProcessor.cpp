#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout PhaseAlignAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY", 1 }, "Delay",
        juce::NormalisableRange<float> { 0.0f, 10.0f, 0.0f, 0.5f }, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "POLARITY", 1 }, "Invert Polarity", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ROTATIONFREQ", 1 }, "Rotation Freq",
        juce::NormalisableRange<float> { 20.0f, 500.0f, 0.0f, 0.4f }, 80.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ROTATIONAMOUNT", 1 }, "Rotation Amount", 0.0f, 1.0f, 0.0f));

    return { params.begin(), params.end() };
}

PhaseAlignAudioProcessor::PhaseAlignAudioProcessor()
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

PhaseAlignAudioProcessor::~PhaseAlignAudioProcessor()
{
}

void PhaseAlignAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxDelayMs / 1000.0 * sampleRate) + 4;
    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);

    rotatorL.reset();
    rotatorR.reset();
}

void PhaseAlignAudioProcessor::releaseResources()
{
}

bool PhaseAlignAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void PhaseAlignAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float delayMs   = apvts.getRawParameterValue ("DELAY")->load();
    const bool polarity   = apvts.getRawParameterValue ("POLARITY")->load() > 0.5f;
    const float rotFreq   = apvts.getRawParameterValue ("ROTATIONFREQ")->load();
    const float rotAmount = apvts.getRawParameterValue ("ROTATIONAMOUNT")->load();

    const float delaySamples = delayMs / 1000.0f * (float) sampleRate;
    const float polaritySign = polarity ? -1.0f : 1.0f;

    auto rotCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass (sampleRate, rotFreq, 0.707f);
    *rotatorL.coefficients = rotCoeffs;
    *rotatorR.coefficients = rotCoeffs;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& line = lines[(size_t) channel];
        const int lineLength = (int) line.size();
        int& wp = writePos[(size_t) channel];
        auto& rotator = (channel == 0) ? rotatorL : rotatorR;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float input = channelData[sample];

            line[(size_t) wp] = input;
            wp = (wp + 1) % lineLength;

            float readPosF = (float) wp - delaySamples;
            while (readPosF < 0.0f)
                readPosF += (float) lineLength;

            const int readIdx0 = (int) readPosF;
            const int readIdx1 = (readIdx0 + 1) % lineLength;
            const float frac = readPosF - (float) readIdx0;

            const float delayed = line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;

            const float rotated = rotator.processSample (delayed);
            const float blended = delayed * (1.0f - rotAmount) + rotated * rotAmount;

            channelData[sample] = blended * polaritySign;
        }
    }
}

juce::AudioProcessorEditor* PhaseAlignAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void PhaseAlignAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PhaseAlignAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhaseAlignAudioProcessor();
}
