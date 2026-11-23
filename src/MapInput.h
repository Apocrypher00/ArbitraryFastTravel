#pragma once

#include "RE/Skyrim.h"

namespace MapInput
{
    // Resolve native input helpers during startup before hooks are enabled.
    void ResolveNativeFunctions();

    // Pass a live, open MapMenu. Suspend only after a dialog is queued.
    // Restore is used by the VR refusal callback. Native travel confirmation
    // restores input itself for No/Place Marker and closes the map for Yes.
    void Suspend(RE::MapMenu* menu);
    void Restore(RE::MapMenu* menu);
}
