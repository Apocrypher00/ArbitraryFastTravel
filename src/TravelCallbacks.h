#pragma once

#include "RE/Skyrim.h"

namespace TravelCallbacks
{
    // Resolve native button handling during startup before enabling hooks.
    void ResolveNativeFunctions();

    // Create callbacks on Skyrim's heap and acquire an owning reference.
    // An empty pointer reports allocation failure. The menu must remain open
    // until response handling. VR travel requires a valid placementPosition.
    RE::BSTSmartPointer<RE::IMessageBoxCallback> CreateTravelCallback(RE::MapMenu* menu, const RE::MenuCursor::RUNTIME_DATA& cursorPosition, const RE::NiPoint3* placementPosition);
    RE::BSTSmartPointer<RE::IMessageBoxCallback> CreateRefusalCallbackVR(RE::MapMenu* menu);
}
