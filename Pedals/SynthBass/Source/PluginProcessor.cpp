#include "PluginProcessor.h"

juce::AudioProcessorValueTreeState::ParameterLayout SynthBassAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "WAVEFORM", 1 }, "Waveform",
        juce::StringArray { "Saw", "Square", "Triangle" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "OCTAVE", 1 }, "Octave", -2, 2, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GLIDE", 1 }, "Glide",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.0f, 0.5f }, 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GATE", 1 }, "Gate", -60.0f, -10.0f, -35.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

SynthBassAudioProcessor::SynthBassAudioProcessor()
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

SynthBassAudioProcessor::~SynthBassAudioProcessor()
{
}

void SynthBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // rango de tracking para bajo: hasta el Mi grave de 5 cuerdas (~31 Hz),
    // ventana mas grande porque las frecuencias bajas necesitan mas
    // muestras para resolver un periodo completo con precision
    pitchTracker.prepare (sampleRate, 4096, 2048, 30.0f, 500.0f);

    oscillator.setSampleRate (sampleRate);
    oscillator.reset();

    smoothedFreqValue = 220.0f;
    currentTargetFrequency = 220.0f;

    // ataque rapido (~5ms), release mas lento (~120ms), como un pedal real
    attackCoeff  = std::exp (-1.0f / (0.005f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.120f * (float) sampleRate));
    envelopeState = 0.0f;
}

void SynthBassAudioProcessor::releaseResources()
{
}

bool SynthBassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SynthBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const int waveform   = (int) apvts.getRawParameterValue ("WAVEFORM")->load();
    const int octave     = (int) apvts.getRawParameterValue ("OCTAVE")->load();
    const float glideMs  = apvts.getRawParameterValue ("GLIDE")->load();
    const float gateDb   = apvts.getRawParameterValue ("GATE")->load();
    const float mix      = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb  = apvts.getRawParameterValue ("LEVEL")->load();

    oscillator.setWaveform (waveform);

    // coeficiente de suavizado exponencial para el glide; se recalcula si
    // el parametro cambio, sin resetear el valor actual (sin saltos)
    const float glideSeconds = juce::jmax (glideMs, 0.1f) / 1000.0f;
    const float glideCoeff = std::exp (-1.0f / (glideSeconds * (float) getSampleRate()));

    const float gateLevel = juce::Decibels::decibelsToGain (gateDb);
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);
    const float octaveMultiplier = std::pow (2.0f, (float) octave);

    // trackeamos el pitch a partir del canal 0 (el pedal es mono por naturaleza)
    const float* trackingChannel = buffer.getReadPointer (0);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryInput = trackingChannel[sample];

        pitchTracker.pushSample (dryInput);

        float newFreq, newRms;
        if (pitchTracker.consumeNewPitch (newFreq, newRms))
        {
            if (newFreq > 0.0f)
            {
                currentTargetFrequency = newFreq * octaveMultiplier;
            }
        }

        // seguidor de envolvente por muestra, para que el ataque/release
        // se sienta suave y no cuantizado al tamaño del hop del pitch tracker
        const float absIn = std::abs (dryInput);
        const float envCoeff = (absIn > envelopeState) ? attackCoeff : releaseCoeff;
        envelopeState = absIn + envCoeff * (envelopeState - absIn);

        const float gateAmount = (envelopeState > gateLevel) ? 1.0f : (envelopeState / juce::jmax (gateLevel, 1.0e-6f));

        smoothedFreqValue = currentTargetFrequency + glideCoeff * (smoothedFreqValue - currentTargetFrequency);
        oscillator.setFrequency (smoothedFreqValue);
        const float synthSample = oscillator.getNextSample() * envelopeState * gateAmount * 3.0f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];
            channelData[sample] = (dry * (1.0f - mix) + synthSample * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* SynthBassAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void SynthBassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SynthBassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthBassAudioProcessor();
}
