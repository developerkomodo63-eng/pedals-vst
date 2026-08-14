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
    class SimplePitchShifter
    {
    public:
        void prepare (double sr, int channels)
        {
            sampleRate = sr;
            numChannels = juce::jmax (1, channels);
            grainSize = juce::jmax (256, (int) std::round (0.035 * sampleRate));
            baseDelay = grainSize * 2;
            const int capacity = grainSize * 8 + 8;
            buffers.resize ((size_t) numChannels);
            for (auto& b : buffers)
            {
                b.assign ((size_t) capacity, 0.0f);
                writePos = 0;
            }
            bufferSize = capacity;
            phase = 0.0f;
            ratio = 1.0f;
        }

        void reset()
        {
            for (auto& b : buffers)
                std::fill (b.begin(), b.end(), 0.0f);
            writePos = 0;
            phase = 0.0f;
        }

        void setPitchRatio (float newRatio) noexcept
        {
            ratio = juce::jlimit (0.5f, 2.0f, newRatio);
        }

        float processSample (int channel, float input) noexcept
        {
            if (buffers.empty())
                return input;

            auto& b = buffers[(size_t) juce::jlimit (0, numChannels - 1, channel)];
            b[(size_t) writePos] = input;

            const float slope = (ratio - 1.0f) * (float) grainSize;
            const float phaseA = phase;
            const float phaseB = std::fmod (phase + 0.5f, 1.0f);
            const float delayA = juce::jlimit (2.0f, (float) bufferSize - 2.0f, (float) baseDelay + (0.5f - phaseA) * slope);
            const float delayB = juce::jlimit (2.0f, (float) bufferSize - 2.0f, (float) baseDelay + (0.5f - phaseB) * slope);

            const float a = readLinear (b, delayA);
            const float c = readLinear (b, delayB);
            const float wa = 1.0f - std::abs (2.0f * phaseA - 1.0f);
            const float wb = 1.0f - std::abs (2.0f * phaseB - 1.0f);
            const float sum = wa + wb + 1.0e-6f;
            const float output = (a * wa + c * wb) / sum;

            writePos = (writePos + 1) % bufferSize;
            phase += (ratio - 1.0f) / (float) grainSize;
            phase -= std::floor (phase);
            return output;
        }

    private:
        float readLinear (const std::vector<float>& b, float delay) const noexcept
        {
            const int size = (int) b.size();
            float readPos = (float) writePos - delay;
            while (readPos < 0.0f) readPos += (float) size;
            while (readPos >= (float) size) readPos -= (float) size;

            const int i0 = (int) readPos;
            const int i1 = (i0 + 1) % size;
            const float frac = readPos - (float) i0;
            return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * frac;
        }

        double sampleRate = 44100.0;
        int numChannels = 2;
        int grainSize = 1543;
        int baseDelay = 3086;
        int bufferSize = 12344;
        int writePos = 0;
        float phase = 0.0f;
        float ratio = 1.0f;
        std::vector<std::vector<float>> buffers;
    };

    SimplePitchShifter shifter;
    struct FormantBank {
        juce::dsp::IIR::Filter<float> f1, f2, f3;
        void reset() { f1.reset(); f2.reset(); f3.reset(); }
    };
    std::vector<FormantBank> formants;
    juce::AudioBuffer<float> dryBuffer;
    double sampleRate=44100.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalShifterAudioProcessor)
};
