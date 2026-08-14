#pragma once

#include "DevKomodoPreset.h"

inline std::vector<DevKomodoPreset> getFuzzGuitarPresets()
{
    return {
        { "Vintage Fuzz",  { { "FUZZ", 0.4f }, { "DRIVE", 15.0f }, { "BIAS", -0.2f }, { "TONE", 3000.0f }, { "MIX", 1.0f }, { "LEVEL", -8.0f } } },
        { "Modern Fuzz",   { { "FUZZ", 0.75f }, { "DRIVE", 35.0f }, { "BIAS", 0.0f },  { "TONE", 4200.0f }, { "MIX", 1.0f }, { "LEVEL", -10.0f } } },
        { "Gated Fuzz",    { { "FUZZ", 1.0f }, { "DRIVE", 70.0f }, { "BIAS", 0.5f },  { "TONE", 3500.0f }, { "MIX", 1.0f }, { "LEVEL", -12.0f } } },
        { "Fuzz Blend",    { { "FUZZ", 0.6f }, { "DRIVE", 25.0f }, { "BIAS", -0.1f }, { "TONE", 3800.0f }, { "MIX", 0.65f }, { "LEVEL", -5.0f } } }
    };
}
