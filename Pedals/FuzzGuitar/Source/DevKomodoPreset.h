#pragma once

#include <JuceHeader.h>

// Una entrada de preset de fabrica: nombre + valores de parametro a
// aplicar. Cada pedal define su propia lista (son distintos parametros
// por pedal), el editor generico solo sabe iterarla y aplicarla.
struct DevKomodoPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values; // {parameterID, value}
};
