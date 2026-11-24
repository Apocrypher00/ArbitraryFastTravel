#pragma once

#include "RE/Skyrim.h"

namespace TravelDialog
{
    // VR restrictions return the localized refusal format to the caller.
    bool ShowRefusalVR(RE::MapMenu* menu, const char* reason, const char* label);

    // The caller has already selected and validated the destination handle.
    // Return false if no confirmation was queued, so the caller can clear it.
    // VR callers also supply the current world-space placement point for Place Marker.
    bool Show(RE::MapMenu* menu, const RE::MapMenuMarker& marker, const RE::NiPoint3* placementPosition = nullptr);
}
