#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout OctaverAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUB1", 1 }, "Sub -1 Oct", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUB2", 1 }, "Sub -2 Oct", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "UP", 1 }, "Octave Up", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRY", 1 }, "Dry Level", 0.0f, 1.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

OctaverAudioProcessor::OctaverAudioProcessor()
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

OctaverAudioProcessor::~OctaverAudioProcessor()
{
}

void OctaverAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    trackingFilter.prepare (spec);
    trackingFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    // guitarra: limpiamos armonicos altos para que el cruce por cero siga
    // al fundamental, no a un armonico
    trackingFilter.setCutoffFrequency (500.0f);

    sub1State = false;
    sub2State = false;
    crossingCounter = 0;
    lastFilteredSample = 0.0f;

    envelopeState = 0.0f;
    attackCoeff  = std::exp (-1.0f / (0.003f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.100f * (float) sampleRate));

    upDcX1 = upDcY1 = 0.0f;
}

void OctaverAudioProcessor::releaseResources()
{
}

bool OctaverAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void OctaverAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();


    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float sub1LevelBase = apvts.getRawParameterValue ("SUB1")->load();
    const float sub1Level = bassMode ? juce::jmin (sub1LevelBase, 0.8f) : sub1LevelBase;
    const float sub2Level = apvts.getRawParameterValue ("SUB2")->load();
    const float upLevelBase   = apvts.getRawParameterValue ("UP")->load();
    const float upLevel   = bassMode ? upLevelBase * 0.45f : upLevelBase;
    const float dryLevel  = apvts.getRawParameterValue ("DRY")->load();
    const float levelDb   = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    const float* trackingChannel = buffer.getReadPointer (0);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dry = trackingChannel[sample];

        const float filtered = trackingFilter.processSample (0, dry);

        // cruce por cero ascendente: alterna los flip-flops que dividen la
        // frecuencia (÷2 = una octava abajo, ÷2 otra vez = dos octavas abajo)
        if (lastFilteredSample <= 0.0f && filtered > 0.0f)
        {
            sub1State = ! sub1State;
            ++crossingCounter;
            if (crossingCounter >= 2)
            {
                sub2State = ! sub2State;
                crossingCounter = 0;
            }
        }
        lastFilteredSample = filtered;

        const float absIn = std::abs (dry);
        const float envCoeff = (absIn > envelopeState) ? attackCoeff : releaseCoeff;
        envelopeState = absIn + envCoeff * (envelopeState - absIn);

        const float sub1Wave = (sub1State ? 1.0f : -1.0f) * envelopeState;
        const float sub2Wave = (sub2State ? 1.0f : -1.0f) * envelopeState;

        // octava arriba: rectificacion de onda completa, tiene energia
        // fuerte al doble de la frecuencia original. Se le saca el DC que
        // deja abs() con un bloqueador de un polo.
        const float rectified = std::abs (dry) * 2.0f;
        const float upX0 = rectified;
        const float upY0 = upX0 - upDcX1 + 0.995f * upDcY1;
        upDcX1 = upX0;
        upDcY1 = upY0;

        const float wetSum = sub1Wave * sub1Level + sub2Wave * sub2Level + upY0 * upLevel;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            channelData[sample] = (channelData[sample] * dryLevel + wetSum) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* OctaverAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void OctaverAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void OctaverAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OctaverAudioProcessor();
}
