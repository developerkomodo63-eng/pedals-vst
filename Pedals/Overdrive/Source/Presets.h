#pragma once

#include "DevKomodoPreset.h"

inline std::vector<DevKomodoPreset> getOverdrivePresets()
{
    return {
        { "Clean Boost",  { { "DRIVE", 2.0f },  { "TONE", 6000.0f }, { "CHARACTER", 0.2f }, { "LEVEL", 3.0f } } },
        { "Blues Crunch", { { "DRIVE", 10.0f }, { "TONE", 4500.0f }, { "CHARACTER", 0.5f }, { "LEVEL", 0.0f } } },
        { "Classic Rock", { { "DRIVE", 20.0f }, { "TONE", 5000.0f }, { "CHARACTER", 0.6f }, { "LEVEL", -2.0f } } },
        { "Mid Push",     { { "DRIVE", 15.0f }, { "TONE", 3500.0f }, { "CHARACTER", 0.75f }, { "LEVEL", -3.0f } } }
    };
}
