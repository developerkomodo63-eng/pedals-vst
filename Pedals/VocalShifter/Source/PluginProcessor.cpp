#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout VocalShifterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"PITCH",1},"Pitch",-12.0f,12.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"FORMANT",1},"Formant",-12.0f,12.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"MODE",1},"Mode",juce::StringArray{"Natural","Robot","Bright","Dark"},0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"DRIVE",1},"Drive",0.0f,12.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"MIX",1},"Mix",0.0f,1.0f,1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"OUTPUT",1},"Output",-18.0f,6.0f,-3.0f));
    return {p.begin(),p.end()};
}
VocalShifterAudioProcessor::VocalShifterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true))
#endif {}
void VocalShifterAudioProcessor::prepareToPlay(double sr,int block)
{
    sampleRate=sr;
    juce::dsp::ProcessSpec spec{sr,(juce::uint32)block,(juce::uint32)getTotalNumOutputChannels()};
    shifter=std::make_unique<juce::dsp::PitchShifter<float>>();
    shifter->prepare(spec);
    shifter->setPitchRatio(1.0f);
    dryBuffer.setSize(getTotalNumOutputChannels(),block,false,false,true);
    formants.resize(getTotalNumOutputChannels());
    for(auto& f:formants) f.reset();
}
void VocalShifterAudioProcessor::releaseResources(){ shifter.reset(); dryBuffer.setSize(0,0); }
bool VocalShifterAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    auto out=l.getMainOutputChannelSet(); return (out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo()) && out==l.getMainInputChannelSet();
}
void VocalShifterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi); juce::ScopedNoDenormals noDenormals;
    const int in=getTotalNumInputChannels(), out=getTotalNumOutputChannels(), n=buffer.getNumSamples();
    for(int c=in;c<out;++c) buffer.clear(c,0,n);
    for(int c=0;c<in;++c) dryBuffer.copyFrom(c,0,buffer,c,0,n);
    const float pitch=apvts.getRawParameterValue("PITCH")->load();
    const float formant=apvts.getRawParameterValue("FORMANT")->load();
    const int mode=(int)apvts.getRawParameterValue("MODE")->load();
    const float drive=apvts.getRawParameterValue("DRIVE")->load();
    const float mix=apvts.getRawParameterValue("MIX")->load();
    const float output=juce::Decibels::decibelsToGain(apvts.getRawParameterValue("OUTPUT")->load());
    float ratio=std::pow(2.0f,pitch/12.0f);
    if(mode==1) ratio=std::pow(2.0f,std::round(pitch)/12.0f);
    shifter->setPitchRatio(ratio);
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    shifter->process(ctx);
    const float formantRatio=std::pow(2.0f,formant/12.0f);
    const float modeHigh=(mode==2?1.8f:(mode==3?0.55f:1.0f));
    const float driveGain=juce::Decibels::decibelsToGain(drive);
    const float formantGain=juce::Decibels::decibelsToGain(formant*0.22f);
    for(int c=0;c<in;++c)
    {
        auto& bank=formants[(size_t)c];
        // Formant section: three gentle resonant bands follow the pitch/formant
        // control instead of merely tilting the whole spectrum. It is intentionally
        // conservative to keep consonants intelligible and avoid ringing.
        const float f1=juce::jlimit(180.0f,(float)sampleRate*0.35f,520.0f*formantRatio);
        const float f2=juce::jlimit(500.0f,(float)sampleRate*0.40f,1450.0f*formantRatio);
        const float f3=juce::jlimit(1200.0f,(float)sampleRate*0.45f,2850.0f*formantRatio);
        *bank.f1.coefficients=juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate,f1,1.0f,formantGain);
        *bank.f2.coefficients=juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate,f2,1.1f,juce::Decibels::decibelsToGain(formant*0.16f));
        *bank.f3.coefficients=juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate,f3,1.2f,juce::Decibels::decibelsToGain(formant*0.10f));
        float* d=buffer.getWritePointer(c); const float* dry=dryBuffer.getReadPointer(c);
        for(int s=0;s<n;++s)
        {
            float wet=bank.f1.processSample(d[s]);
            wet=bank.f2.processSample(wet);
            wet=bank.f3.processSample(wet);
            if(mode==2) wet=std::tanh(wet*1.05f*modeHigh);
            else if(mode==3) wet*=0.86f;
            if(drive>0.001f) wet=std::tanh(wet*driveGain)/std::tanh(driveGain);
            if(mode==1) wet=std::tanh(wet*1.35f);
            d[s]=(dry[s]*(1.0f-mix)+wet*mix)*output;
        }
    }
}
juce::AudioProcessorEditor* VocalShifterAudioProcessor::createEditor(){return new DevKomodoUniversalEditor(*this,apvts,JucePlugin_Name,juce::Colour::fromRGB(210,110,190));}
void VocalShifterAudioProcessor::getStateInformation(juce::MemoryBlock& dest){auto st=apvts.copyState();auto xml=st.createXml();copyXmlToBinary(*xml,dest);}
void VocalShifterAudioProcessor::setStateInformation(const void* data,int size){std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,size));if(xml&&xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new VocalShifterAudioProcessor();}
