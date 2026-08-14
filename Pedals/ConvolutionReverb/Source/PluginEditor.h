#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DevKomodoUI.h"

class ConvolutionReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ConvolutionReverbAudioProcessorEditor (ConvolutionReverbAudioProcessor&);
    ~ConvolutionReverbAudioProcessorEditor() override;
    void resized() override;

private:
    ConvolutionReverbAudioProcessor& audioProcessor;
    juce::TextButton loadButton { "LOAD IR" };
    juce::Label fileLabel;
    std::unique_ptr<DevKomodoUniversalEditor> editor;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void updateFileLabel();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionReverbAudioProcessorEditor)
};
