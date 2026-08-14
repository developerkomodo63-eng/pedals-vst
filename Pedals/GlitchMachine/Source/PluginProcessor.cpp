#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout GlitchMachineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 30.0f, 500.0f, 0.0f, 0.4f }, 150.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CHAOS", 1 }, "Chaos", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

GlitchMachineAudioProcessor::GlitchMachineAudioProcessor()
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

GlitchMachineAudioProcessor::~GlitchMachineAudioProcessor()
{
}

void GlitchMachineAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    rng.setSeedRandomly();

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int bufferLength = (int) (0.6 * sampleRate) + 4; // 600ms de historia alcanza de sobra

    channels.assign ((size_t) numChannels, ChannelState());
    for (auto& c : channels)
    {
        c.captureBuffer.assign ((size_t) bufferLength, 0.0f);
        c.writePos = 0;
        c.slotSamplesRemaining = 0;
        c.currentMode = SlotMode::Passthrough;
    }
}

void GlitchMachineAudioProcessor::releaseResources()
{
}

bool GlitchMachineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GlitchMachineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float rateMs  = apvts.getRawParameterValue ("RATE")->load();
    const float chaos   = apvts.getRawParameterValue ("CHAOS")->load();
    const float mix     = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    const int baseSlotSamples = juce::jmax (16, (int) (rateMs / 1000.0f * (float) sampleRate));

    // opciones cuantizadas de salto de pitch: la mayoria de las veces
    // queda en 1.0 (sin salto), el resto se reparte entre octava/quinta
    // arriba y abajo -- asi el salto, cuando pasa, suena "musical" y no
    // una velocidad al azar
    constexpr float pitchOptions[5] = { 1.0f, 0.5f, 2.0f, 0.75f, 1.5f };

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];
        const int bufferLength = (int) c.captureBuffer.size();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float input = channelData[sample];

            // la captura corre siempre, sin importar el modo del slot actual
            c.captureBuffer[(size_t) c.writePos] = input;

            if (c.slotSamplesRemaining <= 0)
            {
                // jitter en la duracion del slot, mas notorio cuanto mas Chaos
                const float jitter = 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * chaos * 0.4f;
                c.slotSamplesRemaining = juce::jmax (8, (int) ((float) baseSlotSamples * jitter));

                const float roll = rng.nextFloat();
                if (roll < chaos * 0.18f)
                {
                    c.currentMode = SlotMode::Silence;
                }
                else if (roll < chaos * 0.55f)
                {
                    c.currentMode = SlotMode::Glitch;

                    c.segmentLength = juce::jmax (4, (int) ((float) c.slotSamplesRemaining
                                                   * juce::jmap (rng.nextFloat(), 0.15f, 0.7f)));
                    c.segmentLength = juce::jmin (c.segmentLength, bufferLength - 4);

                    c.segmentStart = c.writePos - c.segmentLength;
                    while (c.segmentStart < 0)
                        c.segmentStart += bufferLength;

                    c.direction = (rng.nextFloat() < chaos * 0.4f) ? -1.0f : 1.0f;

                    const int pitchIndex = (rng.nextFloat() < chaos * 0.35f)
                        ? 1 + (int) (rng.nextFloat() * 4.0f)
                        : 0;
                    c.pitchMult = pitchOptions[juce::jlimit (0, 4, pitchIndex)];

                    c.progress = 0.0f;
                }
                else
                {
                    c.currentMode = SlotMode::Passthrough;
                }
            }
            --c.slotSamplesRemaining;

            float wet = input;
            if (c.currentMode == SlotMode::Silence)
            {
                wet = 0.0f;
            }
            else if (c.currentMode == SlotMode::Glitch)
            {
                c.progress += c.pitchMult;
                float localPos = std::fmod (c.progress, (float) c.segmentLength);
                if (c.direction < 0.0f)
                    localPos = (float) c.segmentLength - 1.0f - localPos;

                const int readIndex = (c.segmentStart + (int) localPos) % bufferLength;
                wet = c.captureBuffer[(size_t) readIndex];
            }

            c.writePos = (c.writePos + 1) % bufferLength;

            channelData[sample] = (input * (1.0f - mix) + wet * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* GlitchMachineAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void GlitchMachineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GlitchMachineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchMachineAudioProcessor();
}
