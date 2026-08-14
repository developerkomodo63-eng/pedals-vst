#include "DevKomodoLookAndFeel.h"

const juce::Colour DevKomodoLookAndFeel::background { 0xff1a1714 };
const juce::Colour DevKomodoLookAndFeel::panel      { 0xff262019 };
const juce::Colour DevKomodoLookAndFeel::accent     { 0xffe8862c };
const juce::Colour DevKomodoLookAndFeel::text       { 0xffe8dfd0 };

DevKomodoLookAndFeel::DevKomodoLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, panel);
    setColour (juce::Slider::textBoxOutlineColourId, panel);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.4f));
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::textColourOffId, text);
}

void DevKomodoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPosProportional, float rotaryStartAngle,
                                              float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto radius = diameter * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // cuerpo del knob
    auto knobBounds = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
    g.setColour (panel);
    g.fillEllipse (knobBounds);
    g.setColour (accent.withAlpha (0.25f));
    g.drawEllipse (knobBounds, 1.5f);

    // arco de fondo (rango completo, tenue) y arco de valor (color de marca)
    const float trackRadius = radius * 0.88f;
    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                  rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (text.withAlpha (0.15f));
    g.strokePath (backgroundArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                             rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // puntero
    juce::Path pointer;
    const float pointerLength = radius * 0.62f;
    pointer.startNewSubPath (centre.x, centre.y);
    pointer.lineTo (centre.x + pointerLength * std::sin (angle), centre.y - pointerLength * std::cos (angle));
    g.setColour (text);
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // punto central
    g.setColour (accent);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (centre));
}

void DevKomodoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                              bool shouldDrawButtonAsHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = button.getToggleState();

    g.setColour (on ? accent : panel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (accent.withAlpha (shouldDrawButtonAsHighlighted ? 0.9f : 0.5f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.2f);

    g.setColour (on ? background : text);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::plain)));
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, 1);
}

juce::Font DevKomodoLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (14.0f, juce::Font::plain));
}

juce::Font DevKomodoLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (14.0f, juce::Font::plain));
}
