#pragma once

#include "RE/Skyrim.h"

namespace TravelDialog
{
    // Resolve required native addresses after SKSE initialization and before enabling hooks.
    void ResolveNativeFunctions();

    // Show suspends map controls after queuing the dialog. Native cancellation
    // restores them; RestoreMapInput is retained for explicit recovery if needed.
    // Pass a live, open MapMenu; these helpers do not own its lifetime.
    void SuspendMapInput(RE::MapMenu* menu);
    void RestoreMapInput(RE::MapMenu* menu);

    // The caller has already selected and validated the destination handle.
    // Return false if no confirmation was queued, so the caller can clear it.
    // VR callers also supply the current world-space placement point for Place Marker.
    bool Show(RE::MapMenu* menu, const RE::MapMenuMarker& marker, const RE::NiPoint3* placementPosition = nullptr);
}
