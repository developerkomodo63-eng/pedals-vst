#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout VinylEmulationAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WOW", 1 }, "Wow", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CRACKLE", 1 }, "Crackle", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "AGE", 1 }, "Age", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

VinylEmulationAudioProcessor::VinylEmulationAudioProcessor()
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

VinylEmulationAudioProcessor::~VinylEmulationAudioProcessor()
{
}

void VinylEmulationAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    wowPhase = 0.0f;

    hpFilterL.reset(); hpFilterR.reset();
    lpFilterL.reset(); lpFilterR.reset();

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxLineMs / 1000.0 * sampleRate) + 4;
    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);

    crackleRng.setSeedRandomly();
    crackleEnvelope = 0.0f;
}

void VinylEmulationAudioProcessor::releaseResources()
{
}

bool VinylEmulationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void VinylEmulationAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float wowAmt   = apvts.getRawParameterValue ("WOW")->load();
    const float crackle  = apvts.getRawParameterValue ("CRACKLE")->load();
    const float age      = apvts.getRawParameterValue ("AGE")->load();
    const float mix      = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb  = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    // "Age" angosta el ancho de banda: mas viejo = mas parecido a un
    // parlante de bocina, menos fidelidad
    const float hpFreq = juce::jmap (age, 0.0f, 1.0f, 40.0f, 150.0f);
    const float lpFreq = juce::jmap (age, 0.0f, 1.0f, 15000.0f, 4500.0f);

    auto hpCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, hpFreq);
    auto lpCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, lpFreq);
    *hpFilterL.coefficients = hpCoeffs; *hpFilterR.coefficients = hpCoeffs;
    *lpFilterL.coefficients = lpCoeffs; *lpFilterR.coefficients = lpCoeffs;

    constexpr float wowRateHz = 0.6f;
    constexpr float wowMaxMs = 4.0f; // mas profundo que el Tape: motor de plato, no cabezal
    constexpr float baseLineMs = 4.5f;
    const float wowInc = wowRateHz / (float) sampleRate;

    // probabilidad de que arranque un nuevo "pop" en esta muestra
    const float popProbability = crackle * 0.0015f;
    constexpr float crackleDecay = 0.75f; // pop corto y seco

    for (int sample = 0; sample < numSamples; ++sample)
    {
        wowPhase += wowInc;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;

        const float wowMod = wowAmt * wowMaxMs * std::sin (juce::MathConstants<float>::twoPi * wowPhase);
        const float delayMs = juce::jmax (0.5f, baseLineMs + wowMod);
        const float delaySamples = delayMs / 1000.0f * (float) sampleRate;

        // un solo generador de crackle compartido entre canales (el polvo
        // afecta al surco entero, no a cada canal por separado -- una
        // simplificacion razonable frente al crosstalk real de un vinilo)
        if (crackleRng.nextFloat() < popProbability)
            crackleEnvelope = 0.4f + crackleRng.nextFloat() * 0.6f;
        crackleEnvelope *= crackleDecay;
        const float crackleSample = crackleEnvelope * (crackleRng.nextFloat() * 2.0f - 1.0f);

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

            float wet = (channel == 0) ? hpFilterL.processSample (wobbled) : hpFilterR.processSample (wobbled);
            wet = (channel == 0) ? lpFilterL.processSample (wet) : lpFilterR.processSample (wet);
            wet += crackleSample * 0.15f;

            channelData[sample] = (dry * (1.0f - mix) + wet * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* VinylEmulationAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void VinylEmulationAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void VinylEmulationAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VinylEmulationAudioProcessor();
}
