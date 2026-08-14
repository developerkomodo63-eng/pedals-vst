#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout TremoloAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 0.1f, 20.0f, 0.0f, 0.4f }, 4.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "SHAPE", 1 }, "Shape",
        juce::StringArray { "Sine", "Triangle", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

TremoloAudioProcessor::TremoloAudioProcessor()
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

TremoloAudioProcessor::~TremoloAudioProcessor()
{
}

void TremoloAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    lfoPhase = 0.0f;
    smoothedMod = 1.0f;
}

void TremoloAudioProcessor::releaseResources()
{
}

bool TremoloAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void TremoloAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float rateHz = apvts.getRawParameterValue ("RATE")->load();
    const float depth  = apvts.getRawParameterValue ("DEPTH")->load();
    const int shape    = (int) apvts.getRawParameterValue ("SHAPE")->load();
    const float mix    = apvts.getRawParameterValue ("MIX")->load();

    const float phaseInc = rateHz / (float) sampleRate;

    // suavizado del valor de modulacion (mas fuerte para la onda cuadrada,
    // que si no clickea al saltar de golpe entre los dos niveles)
    const float smoothCoeff = (shape == 2) ? 0.997f : 0.9f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float unipolar;
        switch (shape)
        {
            case 1: // triangular
                unipolar = 1.0f - std::abs (2.0f * lfoPhase - 1.0f);
                break;
            case 2: // cuadrada
                unipolar = (lfoPhase < 0.5f) ? 1.0f : 0.0f;
                break;
            default: // seno
                unipolar = 0.5f * (1.0f + std::sin (juce::MathConstants<float>::twoPi * lfoPhase));
                break;
        }

        const float targetMod = 1.0f - depth * (1.0f - unipolar);
        smoothedMod = targetMod + smoothCoeff * (smoothedMod - targetMod);

        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];
            channelData[sample] = dry * (1.0f - mix) + (dry * smoothedMod) * mix;
        }
    }
}

juce::AudioProcessorEditor* TremoloAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void TremoloAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TremoloAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TremoloAudioProcessor();
}
