#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout PhaserAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 0.02f, 5.0f, 0.0f, 0.4f }, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "STAGES", 1 }, "Stages",
        juce::StringArray { "2", "4", "6", "8" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 }, "Feedback", -0.95f, 0.95f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

    return { params.begin(), params.end() };
}

PhaserAudioProcessor::PhaserAudioProcessor()
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

PhaserAudioProcessor::~PhaserAudioProcessor()
{
}

void PhaserAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    lfoPhase = 0.0f;

    for (auto& channelStages : stages)
        for (auto& stage : channelStages)
            stage = {};

    feedbackState.fill (0.0f);
}

void PhaserAudioProcessor::releaseResources()
{
}

bool PhaserAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void PhaserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();


    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float rateHz     = apvts.getRawParameterValue ("RATE")->load();
    const float depthBase  = apvts.getRawParameterValue ("DEPTH")->load();
    const float depth      = bassMode ? depthBase * 0.6f : depthBase;
    const int stagesChoice = (int) apvts.getRawParameterValue ("STAGES")->load();
    const float feedback   = apvts.getRawParameterValue ("FEEDBACK")->load();
    const float mixBase     = apvts.getRawParameterValue ("MIX")->load();
    const float mix        = bassMode ? juce::jmin (mixBase, 0.6f) : mixBase;

    const int numStages = 2 * (stagesChoice + 1); // choice 0..3 -> 2,4,6,8

    const float phaseInc = rateHz / (float) sampleRate;

    // barrido logaritmico entre ~200Hz y ~2kHz, controlado por el LFO y
    // escalado por Depth (a menos depth, el barrido es mas angosto)
    const float fMin = bassMode ? 450.0f : 200.0f;
    const float fMax = bassMode ? 2600.0f : 2000.0f;
    const float logMin = std::log (fMin);
    const float logMax = std::log (fMax);

    const int channelsToProcess = juce::jmin (totalNumInputChannels, (int) PhaserAudioProcessor::maxChannels);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float lfoUnipolar = 0.5f * (1.0f + std::sin (juce::MathConstants<float>::twoPi * lfoPhase));
        const float depthed = 0.5f - 0.5f * depth + depth * lfoUnipolar; // rango se achica con menos depth
        const float freqHz = std::exp (logMin + (logMax - logMin) * depthed);

        const float tanArg = juce::MathConstants<float>::pi * freqHz / (float) sampleRate;
        const float t = std::tan (tanArg);
        const float coeffA = (t - 1.0f) / (t + 1.0f);

        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];

            const int stateChannel = juce::jmin (channel, channelsToProcess - 1);
            float chainInput = dry + feedbackState[(size_t) stateChannel] * feedback;

            for (int s = 0; s < numStages; ++s)
                chainInput = processAllpass (stages[(size_t) stateChannel][(size_t) s], chainInput, coeffA);

            feedbackState[(size_t) stateChannel] = chainInput;

            channelData[sample] = dry * (1.0f - mix) + chainInput * mix;
        }
    }
}

juce::AudioProcessorEditor* PhaserAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void PhaserAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PhaserAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhaserAudioProcessor();
}
