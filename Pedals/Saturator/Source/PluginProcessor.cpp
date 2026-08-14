#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout SaturatorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"DRIVE",1}, "Drive", 0.0f, 30.0f, 6.0f));
    p.push_back (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"MODE",1}, "Mode", juce::StringArray{"Tube", "Tape", "Diode", "Hard"}, 0));
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"COLOR",1}, "Color", -1.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"MIX",1}, "Mix", 0.0f, 1.0f, 1.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"OUTPUT",1}, "Output", -18.0f, 6.0f, -3.0f));
    return {p.begin(), p.end()};
}
SaturatorAudioProcessor::SaturatorAudioProcessor()
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

void SaturatorAudioProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec{sr, (juce::uint32) block, (juce::uint32) getTotalNumOutputChannels()};
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>((size_t) spec.numChannels, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    oversampler->initProcessing((size_t) block);
    setLatencySamples((int) oversampler->getLatencyInSamples());
    dryBuffer.setSize(getTotalNumOutputChannels(), block, false, false, true);
}
void SaturatorAudioProcessor::releaseResources() { oversampler.reset(); setLatencySamples(0); }
bool SaturatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}
float SaturatorAudioProcessor::saturate (float x, float mode, float character) noexcept
{
    const float bias = character * 0.18f;
    x += bias;
    float y;
    if (mode < 0.5f) y = std::tanh(x * 1.25f);
    else if (mode < 1.5f) y = (2.0f / juce::MathConstants<float>::pi) * std::atan(x * 1.7f);
    else if (mode < 2.5f) y = x / (1.0f + std::abs(x) * 0.85f);
    else y = juce::jlimit(-1.0f, 1.0f, x);
    return y - std::tanh(bias * 1.25f);
}
void SaturatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi); juce::ScopedNoDenormals noDenormals;
    const int in = getTotalNumInputChannels(), out = getTotalNumOutputChannels(), n = buffer.getNumSamples();
    for (int c=in;c<out;++c) buffer.clear(c,0,n);
    const float drive = apvts.getRawParameterValue("DRIVE")->load();
    const float mode = apvts.getRawParameterValue("MODE")->load();
    const float color = apvts.getRawParameterValue("COLOR")->load();
    const float mix = apvts.getRawParameterValue("MIX")->load();
    const float outGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("OUTPUT")->load());
    for (int c = 0; c < in; ++c) dryBuffer.copyFrom(c, 0, buffer, c, 0, n);
    juce::dsp::AudioBlock<float> block(buffer);
    auto os = oversampler->processSamplesUp(block);
    const float gain = juce::Decibels::decibelsToGain(drive);
    for (size_t c=0;c<os.getNumChannels();++c)
    {
        auto* d=os.getChannelPointer(c);
        for (size_t s=0;s<os.getNumSamples();++s)
            d[s]=saturate(d[s]*gain, mode, color);
    }
    oversampler->processSamplesDown(block);
    const float dryGain=1.0f-mix;
    for(int c=0;c<in;++c)
    {
        auto* d=buffer.getWritePointer(c);
        const auto* dry=dryBuffer.getReadPointer(c);
        for(int s=0;s<n;++s) d[s]=(dry[s]*dryGain + d[s]*mix)*outGain;
    }
}
juce::AudioProcessorEditor* SaturatorAudioProcessor::createEditor(){ return new DevKomodoUniversalEditor(*this,apvts,JucePlugin_Name); }
void SaturatorAudioProcessor::getStateInformation(juce::MemoryBlock& dest){ auto st=apvts.copyState(); auto xml=st.createXml(); copyXmlToBinary(*xml,dest); }
void SaturatorAudioProcessor::setStateInformation(const void* data,int size){ std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,size)); if(xml && xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new SaturatorAudioProcessor(); }
