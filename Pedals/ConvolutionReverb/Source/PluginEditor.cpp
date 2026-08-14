#include "PluginEditor.h"

ConvolutionReverbAudioProcessorEditor::ConvolutionReverbAudioProcessorEditor (ConvolutionReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    editor = std::make_unique<DevKomodoUniversalEditor> (p, p.apvts, JucePlugin_Name);

    addAndMakeVisible (loadButton);
    addAndMakeVisible (fileLabel);
    addAndMakeVisible (*editor);

    fileLabel.setJustificationType (juce::Justification::centredLeft);
    fileLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
    updateFileLabel();

    loadButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select a convolution impulse response",
            juce::File(),
            "*.wav;*.aif;*.aiff;*.flac");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioProcessor.loadImpulseResponseFile (file);
                updateFileLabel();
            }
        });
    };

    setSize (800, 550);
}

ConvolutionReverbAudioProcessorEditor::~ConvolutionReverbAudioProcessorEditor() = default;

void ConvolutionReverbAudioProcessorEditor::updateFileLabel()
{
    const auto name = audioProcessor.getLoadedFileName();
    fileLabel.setText (name.isEmpty() ? "IR: built-in / none loaded" : "IR: " + name,
                       juce::dontSendNotification);
}

void ConvolutionReverbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (34).reduced (2, 2);
    loadButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (10);
    fileLabel.setBounds (top);
    area.removeFromTop (4);
    editor->setBounds (area);
}
