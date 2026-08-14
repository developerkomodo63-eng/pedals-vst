#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout EnvelopeFilterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SENSITIVITY", 1 }, "Sensitivity", 0.1f, 5.0f, 1.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MINFREQ", 1 }, "Min Freq",
        juce::NormalisableRange<float> { 100.0f, 1500.0f, 0.0f, 0.4f }, 300.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MAXFREQ", 1 }, "Max Freq",
        juce::NormalisableRange<float> { 500.0f, 4000.0f, 0.0f, 0.4f }, 2500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RESONANCE", 1 }, "Resonance", 0.5f, 8.0f, 3.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "MODE", 1 }, "Mode",
        juce::StringArray { "Lowpass", "Bandpass" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

EnvelopeFilterAudioProcessor::EnvelopeFilterAudioProcessor()
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

EnvelopeFilterAudioProcessor::~EnvelopeFilterAudioProcessor()
{
}

void EnvelopeFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1; // preparamos cada filtro para un solo canal (se procesan por separado)

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    filters.assign ((size_t) numChannels, juce::dsp::StateVariableTPTFilter<float>());
    for (auto& f : filters)
        f.prepare (spec);

    envelopeState = 0.0f;
    attackCoeff  = std::exp (-1.0f / (0.003f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.080f * (float) sampleRate));
}

void EnvelopeFilterAudioProcessor::releaseResources()
{
}

bool EnvelopeFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void EnvelopeFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();


    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float sensitivityBase = apvts.getRawParameterValue ("SENSITIVITY")->load();
    const float sensitivity = bassMode ? sensitivityBase * 0.82f : sensitivityBase;
    const float minFreqBase = apvts.getRawParameterValue ("MINFREQ")->load();
    const float minFreq     = bassMode ? juce::jmax (180.0f, minFreqBase) : minFreqBase;
    const float maxFreqBase = apvts.getRawParameterValue ("MAXFREQ")->load();
    const float maxFreq     = bassMode ? juce::jmax (1200.0f, maxFreqBase) : maxFreqBase;
    const float resonance   = apvts.getRawParameterValue ("RESONANCE")->load();
    const int mode          = (int) apvts.getRawParameterValue ("MODE")->load();
    const float mix         = apvts.getRawParameterValue ("MIX")->load();

    const auto filterType = (mode == 0) ? juce::dsp::StateVariableTPTFilterType::lowpass
                                         : juce::dsp::StateVariableTPTFilterType::bandpass;

    for (auto& f : filters)
    {
        f.setType (filterType);
        f.setResonance (resonance);
    }

    const float logMin = std::log (minFreq);
    const float logMax = std::log (maxFreq);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // la envolvente se calcula sobre el canal 0 y maneja el barrido de
        // todos los canales por igual (como un pedal real, que es mono)
        const float peak = std::abs (buffer.getReadPointer (0)[sample]);
        const float envCoeff = (peak > envelopeState) ? attackCoeff : releaseCoeff;
        envelopeState = peak + envCoeff * (envelopeState - peak);

        const float driven = juce::jlimit (0.0f, 1.0f, envelopeState * sensitivity);
        const float cutoff = std::exp (logMin + (logMax - logMin) * driven);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            filters[(size_t) channel].setCutoffFrequency (cutoff);

            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];
            const float wet = filters[(size_t) channel].processSample (0, dry);
            channelData[sample] = dry * (1.0f - mix) + wet * mix;
        }
    }
}

juce::AudioProcessorEditor* EnvelopeFilterAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void EnvelopeFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EnvelopeFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EnvelopeFilterAudioProcessor();
}
