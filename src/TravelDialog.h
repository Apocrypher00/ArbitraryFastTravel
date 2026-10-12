#pragma once

#include "RE/Skyrim.h"

namespace TravelDialog
{
    // Show suspends map controls after queuing the dialog. Native cancellation
    // restores them; RestoreMapInput is retained for explicit recovery if needed.
    // Pass a live, open MapMenu; these helpers do not own its lifetime.
    void SuspendMapInput(RE::MapMenu* menu);
    void RestoreMapInput(RE::MapMenu* menu);

    // The caller has already selected and validated the destination handle.
    // Return false if no confirmation was queued, so the caller can clear it.
    bool Show(RE::MapMenu* menu, const RE::MapMenuMarker& marker);
}
