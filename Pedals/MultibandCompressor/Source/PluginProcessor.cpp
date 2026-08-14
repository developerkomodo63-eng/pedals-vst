#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout MultibandCompressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "XOVERLOW", 1 }, "Crossover Low",
        juce::NormalisableRange<float> { 100.0f, 500.0f, 0.0f, 0.4f }, 250.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "XOVERHIGH", 1 }, "Crossover High",
        juce::NormalisableRange<float> { 1000.0f, 5000.0f, 0.0f, 0.4f }, 2500.0f));

    const char* bandNames[3] = { "Low", "Mid", "High" };
    const char* bandIds[3]   = { "LOW", "MID", "HIGH" };

    for (int i = 0; i < 3; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { juce::String (bandIds[i]) + "_THRESHOLD", 1 },
            juce::String (bandNames[i]) + " Threshold", -40.0f, 0.0f, -20.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { juce::String (bandIds[i]) + "_RATIO", 1 },
            juce::String (bandNames[i]) + " Ratio",
            juce::NormalisableRange<float> { 1.0f, 10.0f, 0.0f, 0.5f }, 3.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { juce::String (bandIds[i]) + "_MAKEUP", 1 },
            juce::String (bandNames[i]) + " Makeup", 0.0f, 12.0f, 0.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ATTACK", 1 }, "Attack",
        juce::NormalisableRange<float> { 0.1f, 100.0f, 0.0f, 0.4f }, 10.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RELEASE", 1 }, "Release",
        juce::NormalisableRange<float> { 10.0f, 1000.0f, 0.0f, 0.4f }, 130.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

MultibandCompressorAudioProcessor::MultibandCompressorAudioProcessor()
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

MultibandCompressorAudioProcessor::~MultibandCompressorAudioProcessor()
{
}

void MultibandCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    crossovers.resize ((size_t) numChannels);
    for (auto& c : crossovers)
    {
        c.lowSplitLP.prepare (spec);
        c.lowSplitHP.prepare (spec);
        c.highSplitLP.prepare (spec);
        c.highSplitHP.prepare (spec);
        c.lowSplitLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        c.lowSplitHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        c.highSplitLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        c.highSplitHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        c.reset();
    }

    bandEnvelopeDb.fill (-100.0f);
    attackCoeff  = std::exp (-1.0f / (0.010f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.130f * (float) sampleRate));
}

void MultibandCompressorAudioProcessor::releaseResources()
{
}

bool MultibandCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void MultibandCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float xoverLow  = apvts.getRawParameterValue ("XOVERLOW")->load();
    const float xoverHigh = apvts.getRawParameterValue ("XOVERHIGH")->load();

    const std::array<float, 3> thresholds {
        apvts.getRawParameterValue ("LOW_THRESHOLD")->load(),
        apvts.getRawParameterValue ("MID_THRESHOLD")->load(),
        apvts.getRawParameterValue ("HIGH_THRESHOLD")->load()
    };
    const std::array<float, 3> ratios {
        apvts.getRawParameterValue ("LOW_RATIO")->load(),
        apvts.getRawParameterValue ("MID_RATIO")->load(),
        apvts.getRawParameterValue ("HIGH_RATIO")->load()
    };
    const std::array<float, 3> makeups {
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LOW_MAKEUP")->load()),
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("MID_MAKEUP")->load()),
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("HIGH_MAKEUP")->load())
    };

    const float attackMs  = apvts.getRawParameterValue ("ATTACK")->load();
    const float releaseMs = apvts.getRawParameterValue ("RELEASE")->load();
    const float mix       = apvts.getRawParameterValue ("MIX")->load();

    const float sr = (float) getSampleRate();
    attackCoeff  = std::exp (-1.0f / (juce::jmax (attackMs, 0.1f)  / 1000.0f * sr));
    releaseCoeff = std::exp (-1.0f / (juce::jmax (releaseMs, 1.0f) / 1000.0f * sr));

    for (auto& c : crossovers)
    {
        c.lowSplitLP.setCutoffFrequency (xoverLow);
        c.lowSplitHP.setCutoffFrequency (xoverLow);
        c.highSplitLP.setCutoffFrequency (xoverHigh);
        c.highSplitHP.setCutoffFrequency (xoverHigh);
    }

    const int channelsToProcess = totalNumInputChannels;
    std::array<float, 3> bandsPerChannel[2]; // hasta 2 canales, 3 bandas cada uno

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < channelsToProcess && channel < 2; ++channel)
        {
            const float in = buffer.getReadPointer (channel)[sample];
            auto& c = crossovers[(size_t) channel];

            const float low  = c.lowSplitLP.processSample (0, in);
            const float restH = c.lowSplitHP.processSample (0, in);
            const float mid  = c.highSplitLP.processSample (0, restH);
            const float high = c.highSplitHP.processSample (0, restH);

            bandsPerChannel[channel] = { low, mid, high };
        }

        // deteccion y reduccion de ganancia linkeadas al canal 0, aplicadas
        // igual a todos los canales
        std::array<float, 3> bandGain;
        for (int b = 0; b < 3; ++b)
        {
            const float peak = std::abs (bandsPerChannel[0][(size_t) b]);
            const float peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);

            const float envCoeff = (peakDb > bandEnvelopeDb[(size_t) b]) ? attackCoeff : releaseCoeff;
            bandEnvelopeDb[(size_t) b] = peakDb + envCoeff * (bandEnvelopeDb[(size_t) b] - peakDb);

            float gainReductionDb = 0.0f;
            if (bandEnvelopeDb[(size_t) b] > thresholds[(size_t) b])
            {
                const float overDb = bandEnvelopeDb[(size_t) b] - thresholds[(size_t) b];
                gainReductionDb = overDb - overDb / ratios[(size_t) b];
            }

            bandGain[(size_t) b] = juce::Decibels::decibelsToGain (-gainReductionDb) * makeups[(size_t) b];
        }

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            const int srcChannel = juce::jmin (channel, 1);
            const float compressedSum = bandsPerChannel[srcChannel][0] * bandGain[0]
                                       + bandsPerChannel[srcChannel][1] * bandGain[1]
                                       + bandsPerChannel[srcChannel][2] * bandGain[2];

            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];
            channelData[sample] = dry * (1.0f - mix) + compressedSum * mix;
        }
    }
}

juce::AudioProcessorEditor* MultibandCompressorAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void MultibandCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MultibandCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultibandCompressorAudioProcessor();
}
