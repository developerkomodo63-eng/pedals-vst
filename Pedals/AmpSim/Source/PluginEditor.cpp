#include "PluginEditor.h"

AmpSimAudioProcessorEditor::AmpSimAudioProcessorEditor (AmpSimAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    editor = std::make_unique<DevKomodoUniversalEditor> (p, p.apvts, JucePlugin_Name);

    addAndMakeVisible (loadCabButton);
    addAndMakeVisible (clearCabButton);
    addAndMakeVisible (cabFileLabel);
    addAndMakeVisible (*editor);

    cabFileLabel.setJustificationType (juce::Justification::centredLeft);
    cabFileLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
    updateCabLabel();

    loadCabButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select a cabinet impulse response",
            juce::File(),
            "*.wav;*.aif;*.aiff;*.flac");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioProcessor.loadCabIRFile (file);
                updateCabLabel();
            }
        });
    };

    clearCabButton.onClick = [this]
    {
        audioProcessor.clearCabIR();
        updateCabLabel();
    };

    setSize (800, 570);
}

AmpSimAudioProcessorEditor::~AmpSimAudioProcessorEditor() = default;

void AmpSimAudioProcessorEditor::updateCabLabel()
{
    const auto name = audioProcessor.getLoadedCabName();
    cabFileLabel.setText (name.isEmpty() ? "CAB: built-in preset" : "CAB IR: " + name,
                          juce::dontSendNotification);
}

void AmpSimAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (34).reduced (2, 2);
    loadCabButton.setBounds (top.removeFromLeft (112));
    top.removeFromLeft (6);
    clearCabButton.setBounds (top.removeFromLeft (86));
    top.removeFromLeft (10);
    cabFileLabel.setBounds (top);
    area.removeFromTop (4);
    editor->setBounds (area);
}
