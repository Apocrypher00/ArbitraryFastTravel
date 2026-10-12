#pragma once

#include "RE/Skyrim.h"

namespace TravelDialog
{
    // Retained for a future fully modal dialog. Neither is called by the current
    // Yes/No flow. If suspension is enabled, cancellation must restore input.
    // Pass a live, open MapMenu; these helpers do not own its lifetime.
    void SuspendMapInput(RE::MapMenu* menu);
    void RestoreMapInput(RE::MapMenu* menu);

    // The caller has already selected and validated the destination handle.
    // Return false if no confirmation was queued, so the caller can clear it.
    bool Show(RE::MapMenu* menu, const RE::MapMenuMarker& marker);
}
