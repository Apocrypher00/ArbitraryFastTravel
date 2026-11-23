#pragma once

#include "RE/Skyrim.h"

namespace Diagnostics
{
    // Log the selected quest reference and any linked-door arrival data.
    // The caller keeps the resolved reference alive. Inspection runs only at Debug level.
    void LogQuestDestination(const RE::MapMenuMarker& marker, RE::TESObjectREFR* reference);
}
