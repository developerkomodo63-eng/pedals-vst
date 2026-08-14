#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

// Shared, header-only commercial-style editor used by the pedals that do not
// need a bespoke visualizer. It intentionally lives inside each plugin's
// Source directory when installed so the existing CMake/build structure stays
// unchanged.
class DevKomodoUniversalEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    DevKomodoUniversalEditor (juce::AudioProcessor& processor,
                              juce::AudioProcessorValueTreeState& state,
                              juce::String productName,
                              juce::Colour accentColour = juce::Colour::fromRGB (111, 218, 175))
        : AudioProcessorEditor (&processor), processorRef (processor), apvts (state), name (std::move (productName)), accent (accentColour)
    {
        setOpaque (true);
        setResizable (true, true);
        setResizeLimits (560, 330, 1180, 760);
        setSize (760, 470);

        title.setText (name.toUpperCase(), juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        title.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (title);

        brand.setText ("DEVKOMODO", juce::dontSendNotification);
        brand.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        brand.setColour (juce::Label::textColourId, accent.withAlpha (0.9f));
        brand.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (brand);

        presetBox.addItem ("MANUAL", 1);
        factoryPresets = makePresets();
        for (int i = 0; i < (int) factoryPresets.size(); ++i)
            presetBox.addItem (factoryPresets[(size_t) i].name, i + 2);
        presetBox.setSelectedId (1, juce::dontSendNotification);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id >= 2 && id - 2 < (int) factoryPresets.size())
                applyPreset (factoryPresets[(size_t) id - 2]);
        };
        addAndMakeVisible (presetBox);

        if (apvts.getParameter ("INSTRUMENT") != nullptr)
        {
            instrumentButton.setButtonText ("GUITAR");
            instrumentButton.setClickingTogglesState (false);
            instrumentButton.onClick = [this]
            {
                if (auto* p = apvts.getParameter ("INSTRUMENT"))
                {
                    const float current = p->getValue();
                    p->setValueNotifyingHost (current > 0.5f ? 0.0f : 1.0f);
                }
                refreshInstrumentButton();
            };
            instrumentButton.setTooltip ("Switch between Guitar and Bass processing");
            addAndMakeVisible (instrumentButton);
            refreshInstrumentButton();
            startTimerHz (12);
        }

        for (auto* parameter : processorRef.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
            if (ranged == nullptr)
                continue;

            const auto id = ranged->paramID;
            if (id == "INSTRUMENT")
            {
                continue;
            }
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (ranged))
            {
                Choice c;
                c.label = std::make_unique<juce::Label>();
                c.label->setText (ranged->name, juce::dontSendNotification);
                c.label->setJustificationType (juce::Justification::centred);
                c.label->setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                c.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));

                c.box = std::make_unique<juce::ComboBox>();
                c.box->addItemList (choice->choices, 1);
                c.box->setTooltip (ranged->name);
                c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                    (apvts, id, *c.box);

                addAndMakeVisible (*c.label);
                addAndMakeVisible (*c.box);
                choices.push_back (std::move (c));
            }
            else if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
            {
                Toggle t;
                t.button = std::make_unique<juce::ToggleButton> (ranged->name);
                t.button->setTooltip (ranged->name);
                t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                    (apvts, id, *t.button);
                addAndMakeVisible (*t.button);
                toggles.push_back (std::move (t));
            }
            else
            {
                Knob k;
                k.label = std::make_unique<juce::Label>();
                k.label->setText (ranged->name, juce::dontSendNotification);
                k.label->setJustificationType (juce::Justification::centred);
                k.label->setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                k.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));

                k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                            juce::Slider::TextBoxBelow);
                k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 20);
                k.slider->setTooltip (ranged->name);
                k.slider->setDoubleClickReturnValue (true,
                    ranged->getNormalisableRange().convertFrom0to1 (ranged->getDefaultValue()));
                k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                    (apvts, id, *k.slider);

                addAndMakeVisible (*k.label);
                addAndMakeVisible (*k.slider);
                knobs.push_back (std::move (k));
            }
        }

    }

    ~DevKomodoUniversalEditor() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour::fromRGB (13, 14, 18));

        auto outer = getLocalBounds().toFloat().reduced (14.0f);
        g.setColour (juce::Colour::fromRGB (27, 29, 35));
        g.fillRoundedRectangle (outer, 13.0f);
        g.setColour (juce::Colour::fromRGB (55, 58, 68));
        g.drawRoundedRectangle (outer, 13.0f, 1.0f);

        auto header = outer.removeFromTop (64.0f);
        g.setColour (juce::Colour::fromRGB (21, 23, 28));
        g.fillRoundedRectangle (header, 12.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (header.getX(), header.getY(), 4.0f, header.getHeight(), 2.0f);

        auto meter = outer.removeFromTop (34.0f).reduced (10.0f, 7.0f);
        g.setColour (juce::Colour::fromRGB (16, 17, 21));
        g.fillRoundedRectangle (meter, 7.0f);
        g.setColour (juce::Colour::fromRGB (65, 68, 77));
        g.drawRoundedRectangle (meter, 7.0f, 1.0f);

        const float level = 0.12f;
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * level), 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText ("INPUT", meter.reduced (8.0f, 0.0f), juce::Justification::left);

        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText (juce::String (knobs.size() + choices.size() + toggles.size()) + " CONTROLS",
                    meter.reduced (8.0f, 0.0f), juce::Justification::right);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (26, 18);
        auto header = area.removeFromTop (48);

        title.setBounds (header.removeFromLeft (260));
        brand.setBounds (header.removeFromRight (95));
        if (instrumentButton.isVisible())
            instrumentButton.setBounds (header.removeFromRight (112).reduced (3, 8));
        presetBox.setBounds (header.removeFromRight (165).reduced (3, 8));

        area.removeFromTop (36);
        area.reduce (4, 5);

        std::vector<juce::Component*> components;
        std::vector<juce::Component*> labels;
        components.reserve (knobs.size() + choices.size());
        labels.reserve (knobs.size() + choices.size());

        for (auto& k : knobs)
        {
            components.push_back (k.slider.get());
            labels.push_back (k.label.get());
        }
        for (auto& c : choices)
        {
            components.push_back (c.box.get());
            labels.push_back (c.label.get());
        }

        const int count = (int) components.size();
        const int columns = count <= 4 ? count : 4;
        const int rows = juce::jmax (1, (count + columns - 1) / columns);
        const int cellW = juce::jmax (1, area.getWidth() / columns);
        const int cellH = juce::jmax (92, area.getHeight() / rows);

        for (int i = 0; i < count; ++i)
        {
            const int row = i / columns;
            const int col = i % columns;
            auto cell = juce::Rectangle<int> (area.getX() + col * cellW,
                                               area.getY() + row * cellH,
                                               cellW, cellH);
            auto labelArea = cell.removeFromTop (19);
            labels[(size_t) i]->setBounds (labelArea.reduced (5, 0));
            components[(size_t) i]->setBounds (cell.reduced (9, 2));
        }

        if (! toggles.empty())
        {
            auto toggleArea = getLocalBounds().removeFromBottom (34).reduced (28, 4);
            const int w = juce::jmax (1, toggleArea.getWidth() / (int) toggles.size());
            for (auto& t : toggles)
                t.button->setBounds (toggleArea.removeFromLeft (w).reduced (3, 0));
        }
    }

private:
    void timerCallback() override
    {
        refreshInstrumentButton();
    }

    void refreshInstrumentButton()
    {
        if (auto* p = apvts.getParameter ("INSTRUMENT"))
            instrumentButton.setButtonText (p->getValue() > 0.5f ? "BASS" : "GUITAR");
    }

    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Toggle
    {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    static juce::String upper (juce::String value)
    {
        return value.toUpperCase();
    }

    float defaultNormalised (juce::RangedAudioParameter& parameter) const
    {
        return juce::jlimit (0.0f, 1.0f, parameter.getDefaultValue());
    }

    float adjustForPreset (const juce::String& id, float base, int preset) const
    {
        const auto key = upper (id);
        float value = base;

        const bool drive = key.contains ("DRIVE") || key.contains ("FUZZ") || key.contains ("DISTORT")
                        || key.contains ("SATURATION") || key.contains ("BIAS") || key == "GAIN";
        const bool wet = key.contains ("MIX") || key.contains ("DEPTH") || key.contains ("FEEDBACK")
                      || key.contains ("WIDTH") || key.contains ("RESONANCE");
        const bool time = key.contains ("TIME") || key.contains ("DELAY") || key.contains ("RELEASE")
                       || key.contains ("SWELL") || key.contains ("DECAY");
        const bool tone = key.contains ("TONE") || key.contains ("TREBLE") || key.contains ("HIGH")
                       || key.contains ("PRESENCE") || key.contains ("FREQUENCY");
        const bool level = key.contains ("LEVEL") || key.contains ("OUTPUT") || key.contains ("MAKEUP");
        const bool gate = key.contains ("GATE") || key.contains ("THRESHOLD");

        switch (preset)
        {
            case 0: // Factory Reset: defaults.
                break;
            case 1: // Clean / controlled.
                if (drive) value *= 0.45f;
                if (wet) value = juce::jmap (value, 0.0f, 1.0f, 0.15f, 0.45f);
                if (tone) value = juce::jmap (value, 0.0f, 1.0f, 0.42f, 0.62f);
                if (level) value = juce::jmap (value, 0.0f, 1.0f, 0.45f, 0.58f);
                break;
            case 2: // Punch.
                if (drive) value = juce::jmax (value, 0.65f);
                if (wet) value = juce::jlimit (0.0f, 1.0f, value * 1.15f);
                if (time) value *= 0.75f;
                if (gate) value = juce::jlimit (0.0f, 1.0f, value * 0.85f);
                break;
            case 3: // Wide / ambient.
                if (wet) value = juce::jmax (value, 0.72f);
                if (time) value = juce::jmax (value, 0.55f);
                if (tone) value = juce::jmin (1.0f, value * 1.10f);
                break;
            default: // Extreme / modern.
                if (drive) value = juce::jmax (value, 0.82f);
                if (wet) value = juce::jmax (value, 0.80f);
                if (tone) value = juce::jmin (1.0f, value * 1.18f);
                if (level) value = juce::jlimit (0.0f, 1.0f, value * 0.90f);
                break;
        }

        return juce::jlimit (0.0f, 1.0f, value);
    }

    std::vector<Preset> makePresets() const
    {
        juce::StringArray names { "INIT", "CLEAN", "PUNCH", "WIDE", "EXTREME" };
        const auto category = upper (name);
        if (category.contains ("REVERB")) names = juce::StringArray { "INIT", "ROOM", "PLATE", "HALL", "ARENA" };
        else if (category.contains ("DELAY")) names = juce::StringArray { "INIT", "SLAP", "ECHO", "WIDE", "TAPE" };
        else if (category.contains ("CHORUS") || category.contains ("FLANGER") || category.contains ("PHASER") || category.contains ("VIBRATO")) names = juce::StringArray { "INIT", "CLASSIC", "MOTION", "WIDE", "JET" };
        else if (category.contains ("FUZZ") || category.contains ("DISTORT") || category.contains ("OVERDRIVE")) names = juce::StringArray { "INIT", "CRUNCH", "RHYTHM", "LEAD", "HEAVY" };
        else if (category.contains ("COMPRESS")) names = juce::StringArray { "INIT", "GLUE", "PUNCH", "SMOOTH", "LIMIT" };
        else if (category.contains ("EQ")) names = juce::StringArray { "INIT", "TIGHT", "BRIGHT", "PRESENCE", "SCULPT" };
        else if (category.contains ("FILTER")) names = juce::StringArray { "INIT", "FUNK", "QUACK", "SWEEP", "SYNTH" };

        std::vector<Preset> result;
        result.reserve ((size_t) names.size());

        for (int presetIndex = 0; presetIndex < names.size(); ++presetIndex)
        {
            Preset p;
            p.name = names[presetIndex];
            for (auto* parameter : processorRef.getParameters())
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
                if (ranged == nullptr)
                    continue;
                p.values.emplace_back (ranged->paramID,
                                       adjustForPreset (ranged->paramID,
                                                        defaultNormalised (*ranged),
                                                        presetIndex));
            }
            result.push_back (std::move (p));
        }
        return result;
    }

    void applyPreset (const Preset& preset)
    {
        for (const auto& [id, normalised] : preset.values)
            if (auto* parameter = apvts.getParameter (id))
                parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
    }

    juce::AudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;
    juce::String name;
    juce::Colour accent;

    juce::Label title, brand;
    juce::ComboBox presetBox;
    juce::TextButton instrumentButton;
    std::vector<Knob> knobs;
    std::vector<Choice> choices;
    std::vector<Toggle> toggles;
    std::vector<Preset> factoryPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevKomodoUniversalEditor)
};
