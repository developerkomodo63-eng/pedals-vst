#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout ChorusAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 0.05f, 5.0f, 0.0f, 0.4f }, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CENTREDELAY", 1 }, "Centre Delay", 1.0f, 25.0f, 8.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "VOICES", 1 }, "Voices",
        juce::StringArray { "1", "2", "3" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WIDTH", 1 }, "Stereo Width", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 }, "Feedback", -0.9f, 0.9f, 0.1f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

    return { params.begin(), params.end() };
}

ChorusAudioProcessor::ChorusAudioProcessor()
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

ChorusAudioProcessor::~ChorusAudioProcessor()
{
}

void ChorusAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    masterPhase = 0.0f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxLineMs / 1000.0 * sampleRate) + 4;

    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);
}

void ChorusAudioProcessor::releaseResources()
{
}

bool ChorusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ChorusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();


    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float rateHz       = apvts.getRawParameterValue ("RATE")->load();
    const float depthBase    = apvts.getRawParameterValue ("DEPTH")->load();
    const float depth        = bassMode ? depthBase * 0.55f : depthBase;
    const float centreDelayBase = apvts.getRawParameterValue ("CENTREDELAY")->load();
    const float centreDelay  = bassMode ? juce::jmax (6.0f, centreDelayBase) : centreDelayBase;
    const int voicesChoice   = (int) apvts.getRawParameterValue ("VOICES")->load();
    const float width        = apvts.getRawParameterValue ("WIDTH")->load();
    const float feedback     = apvts.getRawParameterValue ("FEEDBACK")->load();
    const float mixBase       = apvts.getRawParameterValue ("MIX")->load();
    const float mix          = bassMode ? juce::jmin (mixBase, 0.55f) : mixBase;

    const int numVoices = voicesChoice + 1; // choice 0..2 -> 1,2,3
    const float phaseInc = rateHz / (float) sampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        masterPhase += phaseInc;
        if (masterPhase >= 1.0f)
            masterPhase -= 1.0f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            auto& line = lines[(size_t) channel];
            const int lineLength = (int) line.size();
            int& wp = writePos[(size_t) channel];

            // desfasa el LFO del canal derecho, asi las voces se mueven
            // distinto entre L y R y el chorus se siente ancho de verdad
            const float channelPhaseOffset = (channel == 1) ? width * 0.25f : 0.0f;

            float wetSum = 0.0f;
            for (int v = 0; v < numVoices; ++v)
            {
                const float voiceOffset = (float) v / (float) numVoices;
                float phase = masterPhase + channelPhaseOffset + voiceOffset;
                phase -= std::floor (phase);

                const float modMs = depth * ChorusAudioProcessor::maxModMs
                                     * std::sin (juce::MathConstants<float>::twoPi * phase);
                const float delayMs = juce::jmax (0.5f, centreDelay + modMs);
                const float delaySamples = delayMs / 1000.0f * (float) sampleRate;

                float readPosF = (float) wp - delaySamples;
                while (readPosF < 0.0f)
                    readPosF += (float) lineLength;

                const int readIdx0 = (int) readPosF;
                const int readIdx1 = (readIdx0 + 1) % lineLength;
                const float frac = readPosF - (float) readIdx0;

                wetSum += line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;
            }

            const float wet = wetSum / (float) numVoices;
            const float input = channelData[sample];

            line[(size_t) wp] = input + wet * feedback;
            wp = (wp + 1) % lineLength;

            channelData[sample] = input * (1.0f - mix) + wet * mix;
        }
    }
}

juce::AudioProcessorEditor* ChorusAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void ChorusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ChorusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChorusAudioProcessor();
}
