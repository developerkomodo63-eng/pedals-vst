#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout TapeEmulationAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WOW", 1 }, "Wow", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FLUTTER", 1 }, "Flutter", 0.0f, 1.0f, 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SATURATION", 1 }, "Saturation", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone",
        juce::NormalisableRange<float> { 3000.0f, 15000.0f, 0.0f, 0.4f }, 9000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "HISS", 1 }, "Hiss", 0.0f, 1.0f, 0.1f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

TapeEmulationAudioProcessor::TapeEmulationAudioProcessor()
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

TapeEmulationAudioProcessor::~TapeEmulationAudioProcessor()
{
}

void TapeEmulationAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    wowPhase = flutterPhase = 0.0f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxLineMs / 1000.0 * sampleRate) + 4;
    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);

    hfRolloffL.reset();
    hfRolloffR.reset();

    // semillas distintas por canal, asi el ruido no queda correlacionado
    // entre L y R (sonaria "mono" y artificial si fuera identico)
    noiseGenL.setSeedRandomly();
    noiseGenR.setSeedRandomly();
}

void TapeEmulationAudioProcessor::releaseResources()
{
}

bool TapeEmulationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float TapeEmulationAudioProcessor::tapeSaturate (float x, float amount) noexcept
{
    // saturacion suave y simetrica, calida, sin el gancho armonico fuerte
    // de un Overdrive -- el objetivo es "compresion/densidad" tipo cinta,
    // no distorsion audible
    const float driven = x * (1.0f + amount * 3.0f);
    const float saturated = std::tanh (driven);
    return x * (1.0f - amount) + saturated * amount;
}

void TapeEmulationAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float wowAmt      = apvts.getRawParameterValue ("WOW")->load();
    const float flutterAmt  = apvts.getRawParameterValue ("FLUTTER")->load();
    const float saturation  = apvts.getRawParameterValue ("SATURATION")->load();
    const float toneHz      = apvts.getRawParameterValue ("TONE")->load();
    const float hiss        = apvts.getRawParameterValue ("HISS")->load();
    const float mix         = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb     = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain  = juce::Decibels::decibelsToGain (levelDb);

    auto toneCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, toneHz);
    *hfRolloffL.coefficients = toneCoeffs;
    *hfRolloffR.coefficients = toneCoeffs;

    constexpr float wowRateHz = 0.7f;
    constexpr float flutterRateHz = 8.0f;
    constexpr float wowMaxMs = 2.5f;
    constexpr float flutterMaxMs = 0.6f;
    constexpr float baseLineMs = 3.0f;

    const float wowInc = wowRateHz / (float) sampleRate;
    const float flutterInc = flutterRateHz / (float) sampleRate;

    // ruido rosa-ish barato: promediamos un par de muestras de ruido blanco,
    // suaviza el espectro sin necesitar un filtro dedicado
    constexpr float noiseLevel = 0.006f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        wowPhase += wowInc;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += flutterInc;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;

        const float wowMod = wowAmt * wowMaxMs * std::sin (juce::MathConstants<float>::twoPi * wowPhase);
        const float flutterMod = flutterAmt * flutterMaxMs * std::sin (juce::MathConstants<float>::twoPi * flutterPhase);
        const float delayMs = juce::jmax (0.5f, baseLineMs + wowMod + flutterMod);
        const float delaySamples = delayMs / 1000.0f * (float) sampleRate;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            auto& line = lines[(size_t) channel];
            const int lineLength = (int) line.size();
            int& wp = writePos[(size_t) channel];

            float readPosF = (float) wp - delaySamples;
            while (readPosF < 0.0f)
                readPosF += (float) lineLength;

            const int readIdx0 = (int) readPosF;
            const int readIdx1 = (readIdx0 + 1) % lineLength;
            const float frac = readPosF - (float) readIdx0;

            const float wobbled = line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;
            const float dry = channelData[sample];

            line[(size_t) wp] = dry;
            wp = (wp + 1) % lineLength;

            float wet = tapeSaturate (wobbled, saturation);
            wet = (channel == 0) ? hfRolloffL.processSample (wet) : hfRolloffR.processSample (wet);

            const float n1 = (channel == 0) ? noiseGenL.nextFloat() : noiseGenR.nextFloat();
            const float n2 = (channel == 0) ? noiseGenL.nextFloat() : noiseGenR.nextFloat();
            wet += ((n1 + n2) - 1.0f) * noiseLevel * hiss;

            channelData[sample] = (dry * (1.0f - mix) + wet * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* TapeEmulationAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void TapeEmulationAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TapeEmulationAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeEmulationAudioProcessor();
}
