#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout RotarySpeakerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "SPEED", 1 }, "Speed",
        juce::StringArray { "Slow", "Fast" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RAMPTIME", 1 }, "Ramp Time", 0.2f, 4.0f, 1.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

RotarySpeakerAudioProcessor::RotarySpeakerAudioProcessor()
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

RotarySpeakerAudioProcessor::~RotarySpeakerAudioProcessor()
{
}

void RotarySpeakerAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    rotorPhase = 0.0f;
    currentRateHz = 0.8f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxLineMs / 1000.0 * sampleRate) + 4;
    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);
}

void RotarySpeakerAudioProcessor::releaseResources()
{
}

bool RotarySpeakerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void RotarySpeakerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const int speedChoice = (int) apvts.getRawParameterValue ("SPEED")->load();
    const float rampTime  = apvts.getRawParameterValue ("RAMPTIME")->load();
    const float depth     = apvts.getRawParameterValue ("DEPTH")->load();
    const float mix       = apvts.getRawParameterValue ("MIX")->load();

    constexpr float slowRateHz = 0.8f;
    constexpr float fastRateHz = 6.5f;
    const float targetRateHz = (speedChoice == 0) ? slowRateHz : fastRateHz;
    const float rampCoeff = std::exp (-1.0f / (rampTime * (float) sampleRate));

    constexpr float maxModMs = 2.5f;
    constexpr float baseLineMs = 3.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // inercia del motor: la velocidad se acerca a la meta, no salta
        currentRateHz += (targetRateHz - currentRateHz) * (1.0f - rampCoeff);

        rotorPhase += currentRateHz / (float) sampleRate;
        if (rotorPhase >= 1.0f)
            rotorPhase -= 1.0f;

        // AM y modulacion de pitch desde la misma fase, 90 grados
        // desfasadas -- asi se relacionan como en un rotor fisico real
        const float amLfo = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::twoPi * rotorPhase));
        const float amGain = 1.0f - depth * 0.5f * (1.0f - amLfo);

        const float pitchLfo = std::sin (juce::MathConstants<float>::twoPi * rotorPhase);
        const float modMs = depth * maxModMs * pitchLfo;
        const float delayMs = juce::jmax (0.5f, baseLineMs + modMs);
        const float delaySamples = delayMs / 1000.0f * (float) sampleRate;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            auto& line = lines[(size_t) channel];
            const int lineLength = (int) line.size();
            int& wp = writePos[(size_t) channel];

            float readPosF = (float) wp - delaySamples;
            while (readPosF < 0.0f)
                readPosF += (float) lineLength;

            const int readIdx0 = (int) readPosF;
            const int readIdx1 = (readIdx0 + 1) % lineLength;
            const float frac = readPosF - (float) readIdx0;

            const float wobbled = line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;
            const float input = channelData[sample];

            line[(size_t) wp] = input;
            wp = (wp + 1) % lineLength;

            const float wet = wobbled * amGain;
            channelData[sample] = input * (1.0f - mix) + wet * mix;
        }
    }
}

juce::AudioProcessorEditor* RotarySpeakerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void RotarySpeakerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RotarySpeakerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RotarySpeakerAudioProcessor();
}
