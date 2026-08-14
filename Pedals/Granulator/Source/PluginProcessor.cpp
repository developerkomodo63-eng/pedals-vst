#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout GranulatorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GRAINSIZE", 1 }, "Grain Size",
        juce::NormalisableRange<float> { 10.0f, 400.0f, 0.0f, 0.4f }, 80.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DENSITY", 1 }, "Density",
        juce::NormalisableRange<float> { 2.0f, 60.0f, 0.0f, 0.4f }, 15.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PITCHSPREAD", 1 }, "Pitch Spread", 0.0f, 12.0f, 3.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "POSITIONJITTER", 1 }, "Position Jitter", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "FREEZE", 1 }, "Freeze", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

GranulatorAudioProcessor::GranulatorAudioProcessor()
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

GranulatorAudioProcessor::~GranulatorAudioProcessor()
{
}

void GranulatorAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    rng.setSeedRandomly();

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int bufferLength = (int) (2.0 * sampleRate) + 4; // 2s de historia

    channels.assign ((size_t) numChannels, ChannelState());
    for (auto& c : channels)
    {
        c.captureBuffer.assign ((size_t) bufferLength, 0.0f);
        c.writePos = 0;
        c.nextGrainSlot = 0;
        c.triggerCountdown = 0;
        for (auto& g : c.grains)
            g.progress = 1.0f;
    }
}

void GranulatorAudioProcessor::releaseResources()
{
}

bool GranulatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GranulatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float grainSizeMs = apvts.getRawParameterValue ("GRAINSIZE")->load();
    const float density     = apvts.getRawParameterValue ("DENSITY")->load();
    const float pitchSpread = apvts.getRawParameterValue ("PITCHSPREAD")->load();
    const float posJitter   = apvts.getRawParameterValue ("POSITIONJITTER")->load();
    const bool freeze        = apvts.getRawParameterValue ("FREEZE")->load() > 0.5f;
    const float mix          = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb      = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain   = juce::Decibels::decibelsToGain (levelDb);

    const float grainSizeSamples = grainSizeMs / 1000.0f * (float) sampleRate;
    const int triggerIntervalSamples = juce::jmax (8, (int) (sampleRate / density));

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];
        const int bufferLength = (int) c.captureBuffer.size();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float input = channelData[sample];

            if (! freeze)
            {
                c.captureBuffer[(size_t) c.writePos] = input;
                c.writePos = (c.writePos + 1) % bufferLength;
            }

            if (c.triggerCountdown <= 0)
            {
                // busca un grano libre; si no hay, roba el mas viejo
                // (mayor progress) para no perder densidad
                int slot = -1;
                float maxProgress = -1.0f;
                for (int g = 0; g < numGrains; ++g)
                {
                    if (c.grains[(size_t) g].progress >= 1.0f) { slot = g; break; }
                    if (c.grains[(size_t) g].progress > maxProgress)
                    {
                        maxProgress = c.grains[(size_t) g].progress;
                        slot = g;
                    }
                }

                auto& grain = c.grains[(size_t) slot];
                const float jitterMs = posJitter * grainSizeMs * 3.0f * rng.nextFloat();
                float startOffset = grainSizeSamples + jitterMs / 1000.0f * (float) sampleRate;
                startOffset = juce::jmin (startOffset, (float) bufferLength - 4.0f);

                float readStart = (float) c.writePos - startOffset;
                while (readStart < 0.0f)
                    readStart += (float) bufferLength;

                grain.readPos = readStart;
                grain.progress = 0.0f;

                const float semitones = (rng.nextFloat() * 2.0f - 1.0f) * pitchSpread;
                grain.pitchMult = std::pow (2.0f, semitones / 12.0f);

                const float intervalJitter = 0.7f + rng.nextFloat() * 0.6f;
                c.triggerCountdown = (int) ((float) triggerIntervalSamples * intervalJitter);
            }
            --c.triggerCountdown;

            float wet = 0.0f;
            for (auto& grain : c.grains)
            {
                if (grain.progress >= 1.0f)
                    continue;

                const int idx0 = (int) grain.readPos % bufferLength;
                const int idx1 = (idx0 + 1) % bufferLength;
                const float frac = grain.readPos - std::floor (grain.readPos);
                const float grainSample = c.captureBuffer[(size_t) idx0] * (1.0f - frac)
                                         + c.captureBuffer[(size_t) idx1] * frac;

                const float window = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * grain.progress));
                wet += grainSample * window;

                grain.readPos += grain.pitchMult;
                while (grain.readPos >= (float) bufferLength)
                    grain.readPos -= (float) bufferLength;
                grain.progress += 1.0f / grainSizeSamples;
            }
            wet *= 0.5f; // normaliza para cuando varios granos se superponen

            channelData[sample] = (input * (1.0f - mix) + wet * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* GranulatorAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void GranulatorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GranulatorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GranulatorAudioProcessor();
}
