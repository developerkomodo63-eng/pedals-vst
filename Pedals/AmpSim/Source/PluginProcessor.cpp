#include "PluginProcessor.h"

juce::AudioProcessorValueTreeState::ParameterLayout AmpSimAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GAIN", 1 }, "Gain",
        juce::NormalisableRange<float> { 1.0f, 30.0f, 0.0f, 0.4f }, 8.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BASS", 1 }, "Bass", -12.0f, 12.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MID", 1 }, "Mid", -12.0f, 12.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TREBLE", 1 }, "Treble", -12.0f, 12.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PRESENCE", 1 }, "Presence", -6.0f, 6.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "CAB", 1 }, "Cabinet",
        juce::StringArray { "Guitar 4x12", "Bass 1x15" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 6.0f, -6.0f));

    return { params.begin(), params.end() };
}

AmpSimAudioProcessor::AmpSimAudioProcessor()
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

AmpSimAudioProcessor::~AmpSimAudioProcessor()
{
}

void AmpSimAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare (spec);
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency (60.0f);

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
        c.reset();

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t) spec.numChannels,
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) oversampler->getLatencyInSamples());

    dcBlockerX1.assign ((size_t) spec.numChannels, 0.0f);
    dcBlockerY1.assign ((size_t) spec.numChannels, 0.0f);
}

void AmpSimAudioProcessor::releaseResources()
{
}

bool AmpSimAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float AmpSimAudioProcessor::preampStage (float x) noexcept
{
    // etapa 1 (preamp a valvula): asimetrica, calida, similar al Overdrive
    constexpr float bias = 0.15f;
    const float biased = x + bias;
    return std::tanh (biased) - std::tanh (bias);
}

float AmpSimAudioProcessor::powerAmpStage (float x) noexcept
{
    // etapa 2 (power amp): curva distinta (racional, no tanh) para que el
    // conjunto de las dos etapas no suene como "un solo tanh mas fuerte" --
    // esto le da mas compresion/sag en los picos, tipico de una etapa de
    // potencia a valvulas cerca de su limite
    return x / (1.0f + std::abs (x));
}

void AmpSimAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float gain      = apvts.getRawParameterValue ("GAIN")->load();
    const float bassDb    = apvts.getRawParameterValue ("BASS")->load();
    const float midDb     = apvts.getRawParameterValue ("MID")->load();
    const float trebleDb  = apvts.getRawParameterValue ("TREBLE")->load();
    const float presenceDb= apvts.getRawParameterValue ("PRESENCE")->load();
    const int cabChoice   = (int) apvts.getRawParameterValue ("CAB")->load();
    const float levelDb   = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    // gabinete de guitarra: mas brillante y con menos low-end util;
    // gabinete de bajo: deja pasar mucho mas grave y corta antes en agudos
    const float cabLowCutFreq  = (cabChoice == 0) ? 90.0f  : 45.0f;
    const float cabHighCutFreq = (cabChoice == 0) ? 5000.0f : 3000.0f;
    const float cabPresenceFreq = (cabChoice == 0) ? 3000.0f : 1200.0f;

    auto bassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        currentSampleRate, 120.0f, 0.7f, juce::Decibels::decibelsToGain (bassDb));
    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, 700.0f, 0.8f, juce::Decibels::decibelsToGain (midDb));
    auto trebleCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        currentSampleRate, 3000.0f, 0.7f, juce::Decibels::decibelsToGain (trebleDb));
    auto cabLowCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
        currentSampleRate, cabLowCutFreq);
    auto cabHighCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (
        currentSampleRate, cabHighCutFreq);
    auto cabPresenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, cabPresenceFreq, 0.9f, juce::Decibels::decibelsToGain (presenceDb));

    for (auto& c : channels)
    {
        *c.bass.coefficients       = *bassCoeffs;
        *c.mid.coefficients        = *midCoeffs;
        *c.treble.coefficients     = *trebleCoeffs;
        *c.cabLowCut.coefficients  = *cabLowCutCoeffs;
        *c.cabHighCut.coefficients = *cabHighCutCoeffs;
        *c.cabPresence.coefficients= *cabPresenceCoeffs;
    }

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
            const float driven = data[sample] * gain;
            const float stage1 = preampStage (driven);
            data[sample] = powerAmpStage (stage1 * 2.0f);
        }
    }

    oversampler->processSamplesDown (block);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];
        float& x1 = dcBlockerX1[(size_t) channel];
        float& y1 = dcBlockerY1[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float y = c.bass.processSample (channelData[sample]);
            y = c.mid.processSample (y);
            y = c.treble.processSample (y);
            y = c.cabLowCut.processSample (y);
            y = c.cabHighCut.processSample (y);
            y = c.cabPresence.processSample (y);

            const float x0 = y;
            const float y0 = x0 - x1 + dcBlockerR * y1;
            x1 = x0;
            y1 = y0;

            channelData[sample] = y0 * outputGain;
        }
    }
}

juce::AudioProcessorEditor* AmpSimAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void AmpSimAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AmpSimAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmpSimAudioProcessor();
}
