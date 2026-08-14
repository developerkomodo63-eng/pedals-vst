#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include <cmath>

// Purpose-built UI for the two audio-to-synth pedals. It keeps the pitch
// tracker visible, makes factory presets immediately useful, and uses the same
// visual language as the rest of the DevKomodo line.
template <typename Processor>
class SynthPedalEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    struct Preset
    {
        const char* name;
        int waveform;
        int octave;
        float glide;
        float detune;
        float sub;
        float gate;
        float mix;
        float level;
    };

    SynthPedalEditor (Processor& p,
                      juce::String titleText,
                      std::array<Preset, 6> presetValues,
                      juce::Colour accentColour)
        : AudioProcessorEditor (&p), processor (p), presets (presetValues), accent (accentColour)
    {
        setOpaque (true);
        setResizable (true, true);
        setResizeLimits (650, 430, 1180, 720);
        setSize (820, 520);

        title.setText (titleText, juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (title);

        subtitle.setText ("MONOPHONIC AUDIO → SYNTH", juce::dontSendNotification);
        subtitle.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        subtitle.setColour (juce::Label::textColourId, accent.withAlpha (0.9f));
        addAndMakeVisible (subtitle);

        presetBox.addItem ("MANUAL", 1);
        for (int i = 0; i < (int) presets.size(); ++i)
            presetBox.addItem (presets[(size_t) i].name, i + 2);
        presetBox.setSelectedId (1, juce::dontSendNotification);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id >= 2 && id - 2 < (int) presets.size())
                loadPreset (presets[(size_t) id - 2]);
        };
        addAndMakeVisible (presetBox);

        configureChoice (waveform, waveformAttachment, "WAVEFORM", { "Saw", "Square", "Triangle" });
        configureSlider (octave, octaveAttachment, "OCTAVE", " oct");
        configureSlider (glide, glideAttachment, "GLIDE", " ms");
        configureSlider (detune, detuneAttachment, "DETUNE", " ct");
        configureSlider (sub, subAttachment, "SUBLEVEL", "");
        configureSlider (gate, gateAttachment, "GATE", " dB");
        configureSlider (mix, mixAttachment, "MIX", "");
        configureSlider (level, levelAttachment, "LEVEL", " dB");

        startTimerHz (20);
    }

    ~SynthPedalEditor() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour::fromRGB (12, 13, 17));

        auto panel = getLocalBounds().toFloat().reduced (14.0f);
        g.setColour (juce::Colour::fromRGB (28, 30, 36));
        g.fillRoundedRectangle (panel, 14.0f);
        g.setColour (juce::Colour::fromRGB (58, 61, 70));
        g.drawRoundedRectangle (panel, 14.0f, 1.0f);

        auto header = panel.removeFromTop (66.0f);
        g.setColour (juce::Colour::fromRGB (20, 22, 27));
        g.fillRoundedRectangle (header, 12.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (header.getX(), header.getY(), 4.0f, header.getHeight(), 2.0f);

        auto tracker = panel.removeFromTop (112.0f).reduced (10.0f, 8.0f);
        g.setColour (juce::Colour::fromRGB (15, 16, 20));
        g.fillRoundedRectangle (tracker, 10.0f);
        g.setColour (juce::Colour::fromRGB (55, 58, 67));
        g.drawRoundedRectangle (tracker, 10.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText ("PITCH TRACKER", tracker.getX() + 14.0f, tracker.getY() + 10.0f,
                    120.0f, 16.0f, juce::Justification::left);

        g.setColour (accent);
        g.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
        g.drawText (detectedNote.isEmpty() ? "--" : detectedNote,
                    tracker.getX() + 14.0f, tracker.getY() + 27.0f,
                    100.0f, 40.0f, juce::Justification::left);

        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (detectedFrequency.isEmpty() ? "-- Hz" : detectedFrequency,
                    tracker.getX() + 118.0f, tracker.getY() + 42.0f,
                    110.0f, 20.0f, juce::Justification::left);

        auto meter = tracker.removeFromRight (180.0f).reduced (10.0f, 42.0f);
        g.setColour (juce::Colour::fromRGB (45, 48, 56));
        g.fillRoundedRectangle (meter, 4.0f);
        const float amount = juce::jlimit (0.0f, 1.0f, processor.getDetectedRms() * 7.0f);
        g.setColour (accent.withAlpha (0.85f));
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * amount), 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
        g.drawText ("INPUT LEVEL", meter.getX(), meter.getY() - 15.0f,
                    meter.getWidth(), 12.0f, juce::Justification::left);

        auto footer = getLocalBounds().removeFromBottom (22).reduced (20, 0);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.setFont (juce::Font (juce::FontOptions (8.0f)));
        g.drawText ("DEVKOMODO  •  FACTORY PRESETS  •  MONO TRACKING",
                    footer, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (28, 18);
        auto header = area.removeFromTop (48);
        title.setBounds (header.removeFromLeft (240));
        subtitle.setBounds (header.removeFromLeft (220).withY (17).withHeight (20));
        presetBox.setBounds (header.removeFromRight (190).withY (6).withHeight (28));

        area.removeFromTop (114);
        area.removeFromBottom (18);
        area.reduce (4, 4);

        auto top = area.removeFromTop (170);
        auto bottom = area;

        waveform.setBounds (top.removeFromLeft (170).reduced (8));
        octave.setBounds (top.removeFromLeft (top.getWidth() / 4).reduced (8));
        glide.setBounds (top.removeFromLeft (top.getWidth() / 3).reduced (8));
        detune.setBounds (top.reduced (8));

        const int bottomW = bottom.getWidth() / 4;
        sub.setBounds (bottom.removeFromLeft (bottomW).reduced (8));
        gate.setBounds (bottom.removeFromLeft (bottomW).reduced (8));
        mix.setBounds (bottom.removeFromLeft (bottomW).reduced (8));
        level.setBounds (bottom.reduced (8));
    }

private:
    template <typename Attachment>
    void configureSlider (juce::Slider& slider,
                          std::unique_ptr<Attachment>& attachment,
                          const char* parameterID,
                          const char* suffix)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        slider.setTextValueSuffix (suffix);
        slider.setName (parameterID);
        slider.setTooltip (parameterID);
        addAndMakeVisible (slider);
        attachment = std::make_unique<Attachment> (processor.apvts, parameterID, slider);
    }

    void configureChoice (juce::ComboBox& box,
                          std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment,
                          const char* parameterID,
                          juce::StringArray choices)
    {
        box.addItemList (choices, 1);
        box.setTooltip (parameterID);
        addAndMakeVisible (box);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
            (processor.apvts, parameterID, box);
    }

    void setParameter (const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (
                parameter->getNormalisableRange().convertTo0to1 (value));
    }

    void loadPreset (const Preset& p)
    {
        setParameter ("WAVEFORM", (float) p.waveform / 2.0f);
        setParameter ("OCTAVE", (float) p.octave);
        setParameter ("GLIDE", p.glide);
        setParameter ("DETUNE", p.detune);
        setParameter ("SUBLEVEL", p.sub);
        setParameter ("GATE", p.gate);
        setParameter ("MIX", p.mix);
        setParameter ("LEVEL", p.level);
    }

    void timerCallback() override
    {
        const float frequency = processor.getDetectedFrequency();
        if (frequency > 0.0f && std::isfinite (frequency))
        {
            detectedFrequency = juce::String (frequency, 1) + " Hz";
            const float midi = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
            const int note = juce::jlimit (0, 127, (int) std::lround (midi));
            static constexpr const char* names[] =
                { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            detectedNote = juce::String (names[note % 12]) + juce::String (note / 12 - 1);
        }
        else
        {
            detectedFrequency = "-- Hz";
            detectedNote.clear();
        }
        repaint();
    }

    Processor& processor;
    std::array<Preset, 6> presets;
    juce::Colour accent;

    juce::Label title, subtitle;
    juce::ComboBox presetBox, waveform;
    juce::Slider octave, glide, detune, sub, gate, mix, level;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;

    juce::String detectedNote;
    juce::String detectedFrequency;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthPedalEditor)
};
