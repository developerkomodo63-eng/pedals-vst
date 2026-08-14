#include "PluginProcessor.h"
#include "SynthEditor.h"
#include <array>
#include <memory>
#include <vector>

juce::AudioProcessorValueTreeState::ParameterLayout SynthGuitarAudioProcessor::createParameterLayout()
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
        juce::ParameterID { "DETUNE", 1 }, "Detune",
        juce::NormalisableRange<float> { 0.0f, 50.0f }, 12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUBLEVEL", 1 }, "Sub Level", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GATE", 1 }, "Gate", -60.0f, -10.0f, -35.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

SynthGuitarAudioProcessor::SynthGuitarAudioProcessor()
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

SynthGuitarAudioProcessor::~SynthGuitarAudioProcessor()
{
}

void SynthGuitarAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // rango de tracking para guitarra: Mi grave (~82 Hz, con margen para
    // afinaciones bajas) hasta bien arriba del diapason
    pitchTracker.prepare (sampleRate, 2048, 256, 55.0f, 900.0f);

    oscillatorMain.setSampleRate (sampleRate);
    oscillatorMain.reset();
    oscillatorUnison.setSampleRate (sampleRate);
    oscillatorUnison.reset();
    oscillatorSub.setSampleRate (sampleRate);
    oscillatorSub.reset();

    smoothedFreqValue = 220.0f;
    currentTargetFrequency = 220.0f;
    hasTrackedPitch = false;

    // ataque rapido (~5ms), release mas lento (~120ms), como un pedal real
    attackCoeff  = std::exp (-1.0f / (0.005f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.120f * (float) sampleRate));
    envelopeState = 0.0f;
    detectedFrequency.store (0.0f);
    detectedRms.store (0.0f);
}

void SynthGuitarAudioProcessor::releaseResources()
{
}

bool SynthGuitarAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SynthGuitarAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const int waveform    = (int) apvts.getRawParameterValue ("WAVEFORM")->load();
    const int octave      = (int) apvts.getRawParameterValue ("OCTAVE")->load();
    const float glideMs   = apvts.getRawParameterValue ("GLIDE")->load();
    const float detuneCts = apvts.getRawParameterValue ("DETUNE")->load();
    const float subLevel  = apvts.getRawParameterValue ("SUBLEVEL")->load();
    const float gateDb    = apvts.getRawParameterValue ("GATE")->load();
    const float mix       = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb   = apvts.getRawParameterValue ("LEVEL")->load();

    oscillatorMain.setWaveform (waveform);
    oscillatorUnison.setWaveform (waveform);
    // el sub siempre suena mejor en cuadrada/senoidal (mas fundamental,
    // menos armonicos altos peleando con la nota principal)
    oscillatorSub.setWaveform (1);

    const float detuneRatio = std::pow (2.0f, detuneCts / 1200.0f);

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
            detectedRms.store (newRms);

            if (newFreq > 0.0f)
            {
                detectedFrequency.store (newFreq);
                currentTargetFrequency = newFreq * octaveMultiplier;

                // Lock the first valid note immediately; subsequent notes use
                // the user-selected glide time. This avoids the synth starting
                // from an arbitrary 220 Hz note on the first pluck.
                if (! hasTrackedPitch)
                {
                    smoothedFreqValue = currentTargetFrequency;
                    hasTrackedPitch = true;
                }
            }
        }

        // seguidor de envolvente por muestra, para que el ataque/release
        // se sienta suave y no cuantizado al tamaño del hop del pitch tracker
        const float absIn = std::abs (dryInput);
        const float envCoeff = (absIn > envelopeState) ? attackCoeff : releaseCoeff;
        envelopeState = absIn + envCoeff * (envelopeState - absIn);

        const float gateAmount = (envelopeState > gateLevel) ? 1.0f : (envelopeState / juce::jmax (gateLevel, 1.0e-6f));

        smoothedFreqValue = currentTargetFrequency + glideCoeff * (smoothedFreqValue - currentTargetFrequency);

        oscillatorMain.setFrequency (smoothedFreqValue);
        oscillatorUnison.setFrequency (smoothedFreqValue * detuneRatio);
        oscillatorSub.setFrequency (smoothedFreqValue * 0.5f);

        // pesos fijos entre voces: principal siempre presente, unisono al
        // 70% (da ancho sin lavar la afinacion), sub escalado por su propio knob
        const float voicesSum = oscillatorMain.getNextSample()
                               + oscillatorUnison.getNextSample() * 0.7f
                               + oscillatorSub.getNextSample() * subLevel;

        const float synthSample = voicesSum * envelopeState * gateAmount * 1.6f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];
            channelData[sample] = (dry * (1.0f - mix) + synthSample * mix) * outputGain;
        }
    }
}


juce::AudioProcessorEditor* SynthGuitarAudioProcessor::createEditor()
{
    static constexpr std::array<SynthPedalEditor<SynthGuitarAudioProcessor>::Preset, 6> presets = {{
        { "Clean Strings", 2, 0, 8.0f, 3.0f, 0.20f, -42.0f, 0.52f, -1.0f },
        { "Synth Lead", 0, 0, 18.0f, 10.0f, 0.10f, -38.0f, 0.78f, -2.0f },
        { "Retro Octave", 1, -1, 25.0f, 8.0f, 0.55f, -39.0f, 0.78f, -2.0f },
        { "Wide Guitar", 0, 0, 42.0f, 24.0f, 0.28f, -36.0f, 0.70f, -3.0f },
        { "Dark Machine", 1, -1, 32.0f, 15.0f, 0.70f, -35.0f, 0.82f, -3.0f },
        { "Bright Pulse", 0, 1, 12.0f, 6.0f, 0.18f, -40.0f, 0.72f, -2.0f }
    }};
    return new SynthPedalEditor<SynthGuitarAudioProcessor> (*this, "SYNTH GUITAR", presets, juce::Colour::fromRGB (112, 184, 255));
}

void SynthGuitarAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SynthGuitarAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthGuitarAudioProcessor();
}
