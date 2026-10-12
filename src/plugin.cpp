#include "PCH.h"
#include "CustomTravel.h"
#include "Hooks.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    // Save the interfaces supplied through the skse pointer, and initialize CommonLib's SKSE API and plugin logger.
    SKSE::Init(skse);

    if (!Hooks::Initialize()) { return false; }

    // Prepare the detour without activating it. Creation only resolves an
    // executable address and builds a trampoline; it does not access game forms.
    // Failures now reject plugin loading before we report successful startup.
    if (!CustomTravel::CreateHook()) {
        Hooks::Reset();
        return false;
    }

    // Activate the prepared hooks together, following WMS's startup order.
    if (!Hooks::EnableAll()) {
        Hooks::Reset();
        return false;
    }

    // All required initialization above completed without returning false.
    SKSE::log::info("ArbitraryFastTravel loaded successfully.");
    return true;
}
