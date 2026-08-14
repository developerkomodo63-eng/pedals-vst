#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout DelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TIME", 1 }, "Time",
        juce::NormalisableRange<float> { 1.0f, 2000.0f, 0.0f, 0.35f }, 350.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 }, "Feedback", 0.0f, 0.95f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone", 500.0f, 12000.0f, 4000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DUCKING", 1 }, "Ducking", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DUCKSENSITIVITY", 1 }, "Duck Sensitivity", 0.2f, 3.0f, 1.0f));

    return { params.begin(), params.end() };
}

DelayAudioProcessor::DelayAudioProcessor()
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

DelayAudioProcessor::~DelayAudioProcessor()
{
}

void DelayAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxDelaySeconds * sampleRate) + 4;

    delayLines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);
    feedbackFilterState.assign ((size_t) numChannels, 0.0f);

    duckEnvelope = 0.0f;
    // ataque rapido (reacciona apenas empezas a tocar), release mas lento
    // (no "bombea" de forma brusca al soltar)
    duckAttackCoeff  = std::exp (-1.0f / (0.008f * (float) sampleRate));
    duckReleaseCoeff = std::exp (-1.0f / (0.250f * (float) sampleRate));
}

void DelayAudioProcessor::releaseResources()
{
}

bool DelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float timeMs   = apvts.getRawParameterValue ("TIME")->load();
    const float feedback = apvts.getRawParameterValue ("FEEDBACK")->load();
    const float toneHz   = apvts.getRawParameterValue ("TONE")->load();
    const float mix      = apvts.getRawParameterValue ("MIX")->load();
    const float ducking  = apvts.getRawParameterValue ("DUCKING")->load();
    const float duckSensitivity = apvts.getRawParameterValue ("DUCKSENSITIVITY")->load();

    const float delaySamples = (timeMs / 1000.0f) * (float) sampleRate;
    const float toneCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * toneHz / (float) sampleRate);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& line = delayLines[(size_t) channel];
        const int lineLength = (int) line.size();
        int& wp = writePos[(size_t) channel];
        float& filterState = feedbackFilterState[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // lectura interpolada linealmente (delay time fraccionario, sin
            // "zipper noise" al mover el knob)
            float readPosF = (float) wp - delaySamples;
            while (readPosF < 0.0f)
                readPosF += (float) lineLength;

            const int readIdx0 = (int) readPosF;
            const int readIdx1 = (readIdx0 + 1) % lineLength;
            const float frac = readPosF - (float) readIdx0;

            const float delayedSample = line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;

            // filtro en el camino de feedback: repeticiones cada vez mas oscuras
            filterState = delayedSample + toneCoeff * (filterState - delayedSample);

            const float input = channelData[sample];
            line[(size_t) wp] = input + filterState * feedback;

            wp = (wp + 1) % lineLength;

            // ducking: solo lo calculamos en el canal 0 y lo aplicamos a
            // todos, para que el eco no se mueva distinto entre L y R
            if (channel == 0)
            {
                const float peak = std::abs (input) * duckSensitivity;
                const float envCoeff = (peak > duckEnvelope) ? duckAttackCoeff : duckReleaseCoeff;
                duckEnvelope = peak + envCoeff * (duckEnvelope - peak);
            }
            const float duckGain = 1.0f - ducking * juce::jlimit (0.0f, 1.0f, duckEnvelope);

            channelData[sample] = input * (1.0f - mix) + delayedSample * mix * duckGain;
        }
    }
}

juce::AudioProcessorEditor* DelayAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void DelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayAudioProcessor();
}
