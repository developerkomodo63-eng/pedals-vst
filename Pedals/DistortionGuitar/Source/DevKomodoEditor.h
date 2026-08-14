#pragma once

#include <JuceHeader.h>
#include "DevKomodoLookAndFeel.h"
#include "DevKomodoPreset.h"

// Editor generico pero con estilo real: en vez de que cada uno de los 50
// pedales tenga una interfaz hecha a mano, este editor lee los parametros
// de la APVTS y arma la grilla de knobs solo, usando el LookAndFeel de
// marca. Cada pedal solo aporta su nombre y su lista de presets -- el
// layout y el dibujado son compartidos. Esto es lo que reemplaza al
// GenericAudioProcessorEditor plano en toda la linea.
class DevKomodoEditor : public juce::AudioProcessorEditor
{
public:
    DevKomodoEditor (juce::AudioProcessor& processorToEdit,
                      juce::AudioProcessorValueTreeState& apvtsToUse,
                      const juce::String& pluginDisplayName,
                      std::vector<DevKomodoPreset> presetsToUse);
    ~DevKomodoEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String displayName;
    std::vector<DevKomodoPreset> presets;

    DevKomodoLookAndFeel lookAndFeel;

    juce::Label titleLabel;
    juce::ComboBox presetBox;

    struct KnobControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<KnobControl> knobs;

    struct ChoiceControl
    {
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    std::vector<ChoiceControl> choices;

    struct ToggleControl
    {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    std::vector<ToggleControl> toggles;

    void applyPreset (int presetIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevKomodoEditor)
};
