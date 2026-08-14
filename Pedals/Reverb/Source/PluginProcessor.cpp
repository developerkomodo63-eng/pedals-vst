#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout ReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "TYPE", 1 }, "Type",
        juce::StringArray { "Room", "Hall", "Plate" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SIZE", 1 }, "Room Size", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DAMPING", 1 }, "Damping", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WIDTH", 1 }, "Width", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER", 1 }, "Shimmer", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "FREEZE", 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

ReverbAudioProcessor::ReverbAudioProcessor()
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

ReverbAudioProcessor::~ReverbAudioProcessor()
{
}

void ReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    reverb.prepare (spec);
    reverb.reset();

    dryBuffer.setSize ((int) spec.numChannels, samplesPerBlock);

    bassWetFilters.assign ((size_t) spec.numChannels, juce::dsp::StateVariableTPTFilter<float>());
    for (auto& f : bassWetFilters)
    {
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        f.setCutoffFrequency (500.0f);
    }

    // buffer del pitch-shifter: 100ms alcanza de sobra para un grano de
    // 40ms leido al doble de velocidad
    const int pitchBufferLength = (int) (0.1 * sampleRate) + 4;
    shimmerState.resize ((size_t) spec.numChannels);
    for (auto& s : shimmerState)
    {
        s.pitchBuffer.assign ((size_t) pitchBufferLength, 0.0f);
        s.writePos = 0;
        s.taps[0] = { 0.0f, 0.0f };
        s.taps[1] = { 0.5f, 0.0f }; // arranca desfasado medio grano del otro tap
    }
    shimmerFeedbackBuffer.setSize ((int) spec.numChannels, samplesPerBlock);
    shimmerFeedbackBuffer.clear();
}

void ReverbAudioProcessor::releaseResources()
{
}

bool ReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float ReverbAudioProcessor::processOctaveUp (ChannelShimmer& state, float input, float grainSizeSamples) noexcept
{
    state.pitchBuffer[(size_t) state.writePos] = input;
    const int bufferLength = (int) state.pitchBuffer.size();

    float output = 0.0f;
    for (auto& tap : state.taps)
    {
        tap.phase += 1.0f / grainSizeSamples;
        if (tap.phase >= 1.0f)
        {
            tap.phase -= 1.0f;
            // reinicia el grano desde justo detras del cabezal de escritura
            tap.readPos = (float) state.writePos - 1.0f;
            if (tap.readPos < 0.0f)
                tap.readPos += (float) bufferLength;
        }

        tap.readPos += 2.0f; // el doble de velocidad = una octava arriba
        while (tap.readPos >= (float) bufferLength)
            tap.readPos -= (float) bufferLength;

        const int idx0 = (int) tap.readPos;
        const int idx1 = (idx0 + 1) % bufferLength;
        const float frac = tap.readPos - (float) idx0;
        const float sampleValue = state.pitchBuffer[(size_t) idx0] * (1.0f - frac)
                                 + state.pitchBuffer[(size_t) idx1] * frac;

        // ventana Hann: los dos granos se cruzan (uno sube mientras el
        // otro baja) para que no haya clicks al reiniciar cada uno
        const float window = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * tap.phase));
        output += sampleValue * window;
    }

    state.writePos = (state.writePos + 1) % bufferLength;
    return output;
}

void ReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();


    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float mix = apvts.getRawParameterValue ("MIX")->load();
    const float bassWetCut = bassMode ? 500.0f : 20.0f;
    const float shimmer = apvts.getRawParameterValue ("SHIMMER")->load();
    const bool freeze = apvts.getRawParameterValue ("FREEZE")->load() > 0.5f;

    const int type = (int) apvts.getRawParameterValue ("TYPE")->load();
    const float rawSize    = apvts.getRawParameterValue ("SIZE")->load();
    const float rawDamping = apvts.getRawParameterValue ("DAMPING")->load();
    const float rawWidth   = apvts.getRawParameterValue ("WIDTH")->load();

    // "Type" no reemplaza los knobs, los reescala: cada tipo tiene un
    // rango/caracter tipico distinto sobre el mismo motor Freeverb, en vez
    // de necesitar tres algoritmos separados
    float mappedSize = rawSize, mappedDamping = rawDamping, mappedWidth = rawWidth;
    switch (type)
    {
        case 1: // Hall: siempre grande, mas brillante (menos damping)
            mappedSize = 0.3f + rawSize * 0.7f;
            mappedDamping = rawDamping * 0.6f;
            break;
        case 2: // Plate: denso, no tan "grande", mas ancho por defecto
            mappedSize = 0.2f + rawSize * 0.3f;
            mappedDamping = 0.3f + rawDamping * 0.4f;
            mappedWidth = juce::jmax (rawWidth, 0.8f);
            break;
        default: // Room: rango normal, algo mas chico en el techo
            mappedSize = rawSize * 0.75f;
            break;
    }

    juce::dsp::Reverb::Parameters params;
    params.roomSize   = mappedSize;
    params.damping    = mappedDamping;
    params.width      = mappedWidth;
    params.freezeMode = freeze ? 1.0f : 0.0f;
    // mezclamos el mix nosotros mismos abajo; el wet/dry interno del
    // algoritmo no es un crossfade lineal
    params.wetLevel = 1.0f;
    params.dryLevel = 0.0f;
    reverb.setParameters (params);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    // metemos el shimmer del bloque anterior (ya desplazado una octava) a
    // la entrada de este bloque, antes de que el reverb lo procese -- asi
    // el loop de shimmer da toda la vuelta por afuera del motor Freeverb
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        const float* feedback = shimmerFeedbackBuffer.getReadPointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] += feedback[sample] * shimmer;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);

    // ahora que ya tenemos la cola de este bloque, la subimos una octava
    // y la guardamos para inyectarla en el bloque que viene
    constexpr float grainSizeMs = 40.0f;
    const float grainSizeSamples = grainSizeMs / 1000.0f * (float) getSampleRate();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float* wetForShimmer = buffer.getReadPointer (channel);
        float* feedbackOut = shimmerFeedbackBuffer.getWritePointer (channel);
        auto& state = shimmerState[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float shifted = processOctaveUp (state, wetForShimmer[sample], grainSizeSamples);
            // limite suave: sin esto, Shimmer alto + Freeze (dos sistemas
            // de feedback apilados) podria acumular volumen sin techo en
            // sesiones largas. tanh no cambia el caracter en uso normal,
            // solo actua como red de seguridad cuando la energia se dispara.
            feedbackOut[sample] = std::tanh (shifted);
        }
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* wet = buffer.getWritePointer (channel);
        const float* dry = dryBuffer.getReadPointer (channel);
        auto& bassFilter = bassWetFilters[(size_t) channel];
        bassFilter.setCutoffFrequency (bassWetCut);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float wetFiltered = bassFilter.processSample (0, wet[sample]);
            wet[sample] = dry[sample] * (1.0f - mix) + wetFiltered * mix;
        }
    }
}

juce::AudioProcessorEditor* ReverbAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name);
}

void ReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbAudioProcessor();
}
