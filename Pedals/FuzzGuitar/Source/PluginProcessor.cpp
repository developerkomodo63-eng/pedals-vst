#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout FuzzGuitarAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FUZZ", 1 }, "Fuzz", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 }, "Drive",
        juce::NormalisableRange<float> { 1.0f, 100.0f, 0.0f, 0.35f }, 25.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BIAS", 1 }, "Bias", -1.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone", 800.0f, 8000.0f, 3500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 6.0f, -6.0f));

    return { params.begin(), params.end() };
}

FuzzGuitarAudioProcessor::FuzzGuitarAudioProcessor()
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

FuzzGuitarAudioProcessor::~FuzzGuitarAudioProcessor()
{
}

void FuzzGuitarAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare(spec);
    hpFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency(100.0f);

    lpFilter.prepare(spec);
    lpFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t) spec.numChannels,
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false);
    oversampler->initProcessing((size_t) samplesPerBlock);
    setLatencySamples ((int) oversampler->getLatencyInSamples());

    dcBlockerX1.assign((size_t) spec.numChannels, 0.0f);
    dcBlockerY1.assign((size_t) spec.numChannels, 0.0f);

    dryBuffer.setSize ((int) spec.numChannels, samplesPerBlock);
}

void FuzzGuitarAudioProcessor::releaseResources()
{
}

bool FuzzGuitarAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float FuzzGuitarAudioProcessor::processFuzzSample (float x, float hardness, float bias) noexcept
{
    const float biased = x + bias * 0.3f;

    // blend entre tanh (fuzz suave, mas redondo) y un clipper tipo diodo
    // (exponencial: se acerca mucho a +-1 sin la "meseta" plana de un clip
    // digital duro, se parece mas a como satura un fuzz real a transistores)
    const float diodeAmount = 1.0f + hardness * 7.0f;
    auto diodeClip = [diodeAmount] (float v) noexcept
    {
        const float sign = (v >= 0.0f) ? 1.0f : -1.0f;
        return sign * (1.0f - std::exp (-diodeAmount * std::abs (v)));
    };

    const float soft = std::tanh (biased);
    const float hard = diodeClip (biased);
    const float shaped = soft * (1.0f - hardness) + hard * hardness;

    const float restBias = bias * 0.3f;
    const float softRest = std::tanh (restBias);
    const float hardRest = diodeClip (restBias);
    const float rest = softRest * (1.0f - hardness) + hardRest * hardness;

    return shaped - rest;
}

void FuzzGuitarAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float fuzzAmountBase = apvts.getRawParameterValue("FUZZ")->load();
    const float fuzzAmount = bassMode ? fuzzAmountBase * 0.78f : fuzzAmountBase;
    const float driveBase   = apvts.getRawParameterValue("DRIVE")->load();
    const float drive       = bassMode ? driveBase * 0.58f : driveBase;
    const float bias        = apvts.getRawParameterValue("BIAS")->load();
    const float toneCutoff  = bassMode ? juce::jmax (900.0f, apvts.getRawParameterValue("TONE")->load()) : apvts.getRawParameterValue("TONE")->load();
    const float mix         = apvts.getRawParameterValue("MIX")->load();
    const float levelDb     = apvts.getRawParameterValue("LEVEL")->load();
    const float outputGain  = juce::Decibels::decibelsToGain(levelDb);

    lpFilter.setCutoffFrequency(toneCutoff);

    const int numSamples = buffer.getNumSamples();

    hpFilter.setCutoffFrequency (bassMode ? 25.0f : 100.0f);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] = hpFilter.processSample (channel, channelData[sample]);
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::AudioBlock<float> oversampledBlock = oversampler->processSamplesUp (block);

    const auto numOSChannels = oversampledBlock.getNumChannels();
    const auto numOSSamples  = oversampledBlock.getNumSamples();

    for (size_t channel = 0; channel < numOSChannels; ++channel)
    {
        float* data = oversampledBlock.getChannelPointer (channel);

        for (size_t sample = 0; sample < numOSSamples; ++sample)
        {
            const float drivenSample = data[sample] * drive;
            data[sample] = processFuzzSample (drivenSample, fuzzAmount, bias);
        }
    }

    oversampler->processSamplesDown (block);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        const float* dry = dryBuffer.getReadPointer (channel);
        float& x1 = dcBlockerX1[(size_t) channel];
        float& y1 = dcBlockerY1[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float toneSample = lpFilter.processSample (channel, channelData[sample]);

            const float x0 = toneSample;
            const float y0 = x0 - x1 + dcBlockerR * y1;
            x1 = x0;
            y1 = y0;

            channelData[sample] = (dry[sample] * (1.0f - mix) + y0 * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* FuzzGuitarAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void FuzzGuitarAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FuzzGuitarAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FuzzGuitarAudioProcessor();
}
