#pragma once

#include "DevKomodoPreset.h"

inline std::vector<DevKomodoPreset> getDistortionGuitarPresets()
{
    return {
        { "Classic Buzz",  { { "DRIVE", 20.0f }, { "SCOOP", 0.15f }, { "BIAS", 0.0f },  { "TONE", 4000.0f }, { "MIX", 1.0f }, { "LEVEL", -4.0f } } },
        { "Heavy Rhythm",  { { "DRIVE", 45.0f }, { "SCOOP", 0.35f }, { "BIAS", 0.1f },  { "TONE", 3200.0f }, { "MIX", 1.0f }, { "LEVEL", -7.0f } } },
        { "Lead Cut",      { { "DRIVE", 60.0f }, { "SCOOP", 0.05f }, { "BIAS", 0.3f },  { "TONE", 5500.0f }, { "MIX", 1.0f }, { "LEVEL", -8.0f } } },
        { "Blend Crunch",  { { "DRIVE", 30.0f }, { "SCOOP", 0.2f },  { "BIAS", 0.0f },  { "TONE", 4200.0f }, { "MIX", 0.6f }, { "LEVEL", -2.0f } } }
    };
}
