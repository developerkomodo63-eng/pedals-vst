#pragma once

#include <JuceHeader.h>

// Look and feel compartido de toda la linea DevKomodo. Vive en cada
// proyecto de pedal como una copia (los pedales son proyectos JUCE
// independientes por diseño, para que el CI compile cada uno aislado),
// pero el codigo es identico en todos -- es el "kit de piezas" visual
// de la marca, no un tema distinto por pedal.
class DevKomodoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DevKomodoLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle,
                            float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    static const juce::Colour background;
    static const juce::Colour panel;
    static const juce::Colour accent;
    static const juce::Colour text;
};
