#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout SpringReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 }, "Decay", 0.0f, 0.95f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone",
        juce::NormalisableRange<float> { 1500.0f, 8000.0f, 0.0f, 0.4f }, 4000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BOING", 1 }, "Boing", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

SpringReverbAudioProcessor::SpringReverbAudioProcessor()
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

SpringReverbAudioProcessor::~SpringReverbAudioProcessor()
{
}

void SpringReverbAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    lfoPhase = 0.0f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);

    for (auto& c : channels)
    {
        for (int i = 0; i < numCombs; ++i)
        {
            // +8ms de margen extra para poder modular el delay sin salirse del buffer
            const int len = (int) ((combTimesMs[(size_t) i] + 8.0f) / 1000.0 * sampleRate) + 4;
            c.combLines[(size_t) i].assign ((size_t) len, 0.0f);
        }

        const int ap1Len = (int) (5.0 / 1000.0 * sampleRate) + 2;
        const int ap2Len = (int) (1.7 / 1000.0 * sampleRate) + 2;
        c.allpass1Line.assign ((size_t) ap1Len, 0.0f);
        c.allpass2Line.assign ((size_t) ap2Len, 0.0f);

        c.reset();
    }
}

void SpringReverbAudioProcessor::releaseResources()
{
}

bool SpringReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SpringReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float decay   = apvts.getRawParameterValue ("DECAY")->load();
    const float toneHz  = apvts.getRawParameterValue ("TONE")->load();
    const float boing   = apvts.getRawParameterValue ("BOING")->load();
    const float mix     = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    auto hpCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, 250.0f);
    auto lpCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, toneHz);
    for (auto& c : channels)
    {
        *c.hpFilter.coefficients = hpCoeffs;
        *c.lpFilter.coefficients = lpCoeffs;
    }

    constexpr float lfoRateHz = 0.6f;
    constexpr float allpassG = 0.6f;
    const float lfoInc = lfoRateHz / (float) sampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            auto& c = channels[(size_t) channel];
            const float dry = channelData[sample];

            float combSum = 0.0f;
            for (int i = 0; i < numCombs; ++i)
            {
                auto& line = c.combLines[(size_t) i];
                const int lineLength = (int) line.size();
                int& wp = c.combWritePos[(size_t) i];

                // cada comb se modula un poquito distinto (fase desfasada
                // por indice) para que el "boing" no suene sincronizado/plano
                const float modMs = boing * 1.5f * std::sin (juce::MathConstants<float>::twoPi * lfoPhase + (float) i * 2.1f);
                const float delaySamples = (combTimesMs[(size_t) i] + modMs) / 1000.0f * (float) sampleRate;

                float readPosF = (float) wp - delaySamples;
                while (readPosF < 0.0f)
                    readPosF += (float) lineLength;

                const int readIdx0 = (int) readPosF;
                const int readIdx1 = (readIdx0 + 1) % lineLength;
                const float frac = readPosF - (float) readIdx0;
                const float delayed = line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;

                line[(size_t) wp] = dry + delayed * decay;
                wp = (wp + 1) % lineLength;

                combSum += delayed;
            }
            combSum /= (float) numCombs;

            // difusion: dos allpass Schroeder en serie, tiempos fijos
            float apOut = combSum;
            {
                auto& line = c.allpass1Line;
                const int len = (int) line.size();
                int& wp = c.allpass1WritePos;
                const float bufOut = line[(size_t) wp];
                const float apIn = apOut;
                const float y = -allpassG * apIn + bufOut;
                line[(size_t) wp] = apIn + allpassG * y;
                wp = (wp + 1) % len;
                apOut = y;
            }
            {
                auto& line = c.allpass2Line;
                const int len = (int) line.size();
                int& wp = c.allpass2WritePos;
                const float bufOut = line[(size_t) wp];
                const float apIn = apOut;
                const float y = -allpassG * apIn + bufOut;
                line[(size_t) wp] = apIn + allpassG * y;
                wp = (wp + 1) % len;
                apOut = y;
            }

            float toned = c.hpFilter.processSample (apOut);
            toned = c.lpFilter.processSample (toned);

            channelData[sample] = (dry * (1.0f - mix) + toned * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* SpringReverbAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void SpringReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpringReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpringReverbAudioProcessor();
}
