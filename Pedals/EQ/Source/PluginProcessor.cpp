#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout EQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // banda 0: low shelf
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BAND0_FREQ", 1 }, "Band 1 Freq (Low Shelf)",
        juce::NormalisableRange<float> { 30.0f, 500.0f, 0.0f, 0.4f }, 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BAND0_GAIN", 1 }, "Band 1 Gain", -12.0f, 12.0f, 0.0f));

    // bandas 1-4: peak, cada una con su propia frecuencia ajustable
    struct PeakDefault { float freq, freqMin, freqMax; };
    constexpr PeakDefault peakDefaults[4] = {
        { 250.0f,  100.0f,  1000.0f },
        { 800.0f,  300.0f,  3000.0f },
        { 2000.0f, 800.0f,  6000.0f },
        { 4000.0f, 1500.0f, 9000.0f }
    };

    for (int i = 0; i < 4; ++i)
    {
        const int bandIndex = i + 1;
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "BAND" + juce::String (bandIndex) + "_FREQ", 1 },
            "Band " + juce::String (bandIndex + 1) + " Freq",
            juce::NormalisableRange<float> { peakDefaults[i].freqMin, peakDefaults[i].freqMax, 0.0f, 0.4f },
            peakDefaults[i].freq));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "BAND" + juce::String (bandIndex) + "_GAIN", 1 },
            "Band " + juce::String (bandIndex + 1) + " Gain", -12.0f, 12.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "BAND" + juce::String (bandIndex) + "_Q", 1 },
            "Band " + juce::String (bandIndex + 1) + " Q", 0.3f, 5.0f, 1.0f));
    }

    // banda 5: high shelf
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BAND5_FREQ", 1 }, "Band 6 Freq (High Shelf)",
        juce::NormalisableRange<float> { 2000.0f, 16000.0f, 0.0f, 0.4f }, 8000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BAND5_GAIN", 1 }, "Band 6 Gain", -12.0f, 12.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

EQAudioProcessor::EQAudioProcessor()
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

EQAudioProcessor::~EQAudioProcessor()
{
}

void EQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    bands.resize ((size_t) numChannels);
    for (auto& b : bands)
        b.reset();
}

void EQAudioProcessor::releaseResources()
{
}

bool EQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void EQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    std::array<std::array<float, 6>, numBands> coeffs;

    coeffs[0] = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (
        currentSampleRate,
        apvts.getRawParameterValue ("BAND0_FREQ")->load(), 0.7f,
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("BAND0_GAIN")->load()));

    const char* freqIds[] = { "BAND1_FREQ", "BAND2_FREQ", "BAND3_FREQ", "BAND4_FREQ" };
    const char* qIds[]    = { "BAND1_Q", "BAND2_Q", "BAND3_Q", "BAND4_Q" };
    const char* gainIds[] = { "BAND1_GAIN", "BAND2_GAIN", "BAND3_GAIN", "BAND4_GAIN" };

    for (int i = 1; i <= 4; ++i)
    {
        const int index = i - 1;
        coeffs[(size_t) i] = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
            currentSampleRate,
            apvts.getRawParameterValue (freqIds[index])->load(),
            apvts.getRawParameterValue (qIds[index])->load(),
            juce::Decibels::decibelsToGain (apvts.getRawParameterValue (gainIds[index])->load()));
    }

    coeffs[5] = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
        currentSampleRate,
        apvts.getRawParameterValue ("BAND5_FREQ")->load(), 0.7f,
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("BAND5_GAIN")->load()));

    for (auto& b : bands)
        for (int i = 0; i < numBands; ++i)
            *b.filters[(size_t) i].coefficients = coeffs[(size_t) i];

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& b = bands[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float y = channelData[sample];
            for (int i = 0; i < numBands; ++i)
                y = b.filters[(size_t) i].processSample (y);
            channelData[sample] = y * outputGain;
        }
    }
}

juce::AudioProcessorEditor* EQAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void EQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQAudioProcessor();
}
