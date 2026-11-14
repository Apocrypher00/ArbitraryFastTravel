#pragma once

namespace MarkerSelectionOverride
{
    // Resolve required native addresses after SKSE initialization and before enabling hooks.
    void ResolveNativeFunctions();

    // Prepare the MarkerClick detour. Plugin startup activates it after setup succeeds.
    bool CreateHook();
}
