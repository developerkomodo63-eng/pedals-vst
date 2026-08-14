#pragma once
#include <JuceHeader.h>

class VocalShifterAudioProcessor final : public juce::AudioProcessor
{
public:
    VocalShifterAudioProcessor();
    ~VocalShifterAudioProcessor() override = default;
    void prepareToPlay(double sampleRate,int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{*this,nullptr,"Parameters",createParameterLayout()};
private:
    std::unique_ptr<juce::dsp::PitchShifter<float>> shifter;
    struct FormantBank {
        juce::dsp::IIR::Filter<float> f1, f2, f3;
        void reset() { f1.reset(); f2.reset(); f3.reset(); }
    };
    std::vector<FormantBank> formants;
    juce::AudioBuffer<float> dryBuffer;
    double sampleRate=44100.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalShifterAudioProcessor)
};
