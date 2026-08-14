#include "DevKomodoEditor.h"

DevKomodoEditor::DevKomodoEditor (juce::AudioProcessor& processorToEdit,
                                   juce::AudioProcessorValueTreeState& apvtsToUse,
                                   const juce::String& pluginDisplayName,
                                   std::vector<DevKomodoPreset> presetsToUse)
    : AudioProcessorEditor (&processorToEdit),
      apvts (apvtsToUse),
      displayName (pluginDisplayName),
      presets (std::move (presetsToUse))
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText (displayName, juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, DevKomodoLookAndFeel::accent);
    addAndMakeVisible (titleLabel);

    presetBox.addItem ("Manual", 1);
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 2);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 2)
            applyPreset (id - 2);
    };
    addAndMakeVisible (presetBox);

    // recorremos los parametros de la APVTS en el orden en que se
    // crearon (que ya viene pensado como un orden logico de izquierda a
    // derecha) y armamos el control que corresponda segun el tipo
    for (auto* param : processorToEdit.getParameters())
    {
        auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (rangedParam == nullptr)
            continue;

        const auto paramID = rangedParam->paramID;

        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (rangedParam))
        {
            ChoiceControl control;
            control.box = std::make_unique<juce::ComboBox>();
            control.box->addItemList (choiceParam->choices, 1);
            control.label = std::make_unique<juce::Label>();
            control.label->setText (rangedParam->name, juce::dontSendNotification);
            control.label->setJustificationType (juce::Justification::centred);
            control.label->setFont (juce::Font (juce::FontOptions (12.0f)));
            control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                (apvts, paramID, *control.box);

            addAndMakeVisible (*control.box);
            addAndMakeVisible (*control.label);
            choices.push_back (std::move (control));
        }
        else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*> (rangedParam))
        {
            juce::ignoreUnused (boolParam);
            ToggleControl control;
            control.button = std::make_unique<juce::ToggleButton>();
            control.button->setButtonText (rangedParam->name);
            control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                (apvts, paramID, *control.button);

            addAndMakeVisible (*control.button);
            toggles.push_back (std::move (control));
        }
        else
        {
            KnobControl control;
            control.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                               juce::Slider::TextBoxBelow);
            control.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
            control.label = std::make_unique<juce::Label>();
            control.label->setText (rangedParam->name, juce::dontSendNotification);
            control.label->setJustificationType (juce::Justification::centred);
            control.label->setFont (juce::Font (juce::FontOptions (12.0f)));
            control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                (apvts, paramID, *control.slider);

            addAndMakeVisible (*control.slider);
            addAndMakeVisible (*control.label);
            knobs.push_back (std::move (control));
        }
    }

    // tamaño segun cuantos knobs hay que entrar (4 por fila), asi los
    // pedales con mas parametros no quedan apretados
    const int totalKnobLikeControls = (int) knobs.size() + (int) choices.size();
    const int columns = 4;
    const int rows = juce::jmax (1, (totalKnobLikeControls + columns - 1) / columns);
    const int extraRowsForToggles = toggles.empty() ? 0 : 1;

    setSize (560, 90 + rows * 110 + extraRowsForToggles * 40 + 20);
}

DevKomodoEditor::~DevKomodoEditor()
{
    setLookAndFeel (nullptr);
}

void DevKomodoEditor::applyPreset (int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= (int) presets.size())
        return;

    for (const auto& [paramID, value] : presets[(size_t) presetIndex].values)
    {
        if (auto* param = apvts.getParameter (paramID))
        {
            const auto normalised = param->getNormalisableRange().convertTo0to1 (value);
            param->setValueNotifyingHost (normalised);
        }
    }
}

void DevKomodoEditor::paint (juce::Graphics& g)
{
    g.fillAll (DevKomodoLookAndFeel::background);

    auto headerArea = getLocalBounds().removeFromTop (70);
    g.setColour (DevKomodoLookAndFeel::panel);
    g.fillRect (headerArea);

    g.setColour (DevKomodoLookAndFeel::accent.withAlpha (0.5f));
    g.drawLine (0.0f, 70.0f, (float) getWidth(), 70.0f, 1.5f);

    g.setColour (DevKomodoLookAndFeel::text.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("DevKomodo", getLocalBounds().removeFromTop (70).reduced (12, 8),
                juce::Justification::bottomLeft);
}

void DevKomodoEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (70).reduced (16, 10);
    titleLabel.setBounds (header.removeFromLeft (header.getWidth() - 160));
    header.removeFromLeft (10);
    presetBox.setBounds (header.removeFromTop (28));

    area.reduce (16, 10);

    if (! toggles.empty())
    {
        auto toggleRow = area.removeFromTop (32);
        const int toggleWidth = toggleRow.getWidth() / (int) toggles.size();
        for (auto& t : toggles)
        {
            t.button->setBounds (toggleRow.removeFromLeft (toggleWidth).reduced (4, 0));
        }
        area.removeFromTop (8);
    }

    constexpr int columns = 4;
    const int cellWidth = area.getWidth() / columns;
    constexpr int cellHeight = 110;

    int index = 0;
    auto placeNext = [&] (juce::Component& comp, juce::Component* labelComp)
    {
        const int col = index % columns;
        const int row = index / columns;
        auto cell = juce::Rectangle<int> (area.getX() + col * cellWidth, area.getY() + row * cellHeight,
                                           cellWidth, cellHeight);
        if (labelComp != nullptr)
        {
            auto labelArea = cell.removeFromTop (18);
            labelComp->setBounds (labelArea);
        }
        comp.setBounds (cell.reduced (10, 4));
        ++index;
    };

    for (auto& k : knobs)
        placeNext (*k.slider, k.label.get());

    for (auto& c : choices)
        placeNext (*c.box, c.label.get());
}
