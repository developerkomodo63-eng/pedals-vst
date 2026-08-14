#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout OverdriveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 }, "Drive", 1.0f, 40.0f, 12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone", 1000.0f, 8000.0f, 4500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CHARACTER", 1 }, "Character", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 6.0f, 0.0f));

    return { params.begin(), params.end() };
}

OverdriveAudioProcessor::OverdriveAudioProcessor()
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

OverdriveAudioProcessor::~OverdriveAudioProcessor()
{
}

void OverdriveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare(spec);
    hpFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency(25.0f);

    lpFilter.prepare(spec);
    lpFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    // 2x oversampling solo alrededor de la saturacion, para no pagar el
    // costo de CPU en toda la cadena. IIR polifase = liviano, sin fase lineal.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t) spec.numChannels,
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false);
    oversampler->initProcessing((size_t) samplesPerBlock);
    setLatencySamples ((int) oversampler->getLatencyInSamples());

    dcBlockerX1.assign((size_t) spec.numChannels, 0.0f);
    dcBlockerY1.assign((size_t) spec.numChannels, 0.0f);
}

void OverdriveAudioProcessor::releaseResources()
{
}

bool OverdriveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float OverdriveAudioProcessor::processSaturationSample (float x, float character) noexcept
{
    // Character controla la asimetria: en 0 casi simetrico (mas tenso,
    // mas parecido a un op-amp), en 1 bien sesgado (mas parecido a una
    // sola valvula, mas 2do armonico). 0.12 era el valor fijo anterior;
    // ahora es el techo del rango, no un numero cerrado.
    const float bias = character * 0.24f;
    const float biased = x + bias;
    const float cubic = biased - (biased * biased * biased) * 0.14f;
    const float shaped = std::tanh(cubic);

    return shaped - std::tanh(bias - (bias * bias * bias) * 0.14f);
}

void OverdriveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float driveBase = apvts.getRawParameterValue("DRIVE")->load();
    const float drive = bassMode ? driveBase * 0.72f : driveBase;
    const float toneCutoff = bassMode ? juce::jmax (900.0f, apvts.getRawParameterValue("TONE")->load()) : apvts.getRawParameterValue("TONE")->load();
    const float character = apvts.getRawParameterValue("CHARACTER")->load();
    const float levelDb = apvts.getRawParameterValue("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain(levelDb);

    lpFilter.setCutoffFrequency(toneCutoff);

    const int numSamples = buffer.getNumSamples();


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
            data[sample] = processSaturationSample (drivenSample, character);
        }
    }

    oversampler->processSamplesDown (block);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        float& x1 = dcBlockerX1[(size_t) channel];
        float& y1 = dcBlockerY1[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float toneSample = lpFilter.processSample (channel, channelData[sample]);

            // DC blocker de un polo: y[n] = x[n] - x[n-1] + R*y[n-1]
            const float x0 = toneSample;
            const float y0 = x0 - x1 + dcBlockerR * y1;
            x1 = x0;
            y1 = y0;

            channelData[sample] = y0 * outputGain;
        }
    }
}

juce::AudioProcessorEditor* OverdriveAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void OverdriveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void OverdriveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OverdriveAudioProcessor();
}
