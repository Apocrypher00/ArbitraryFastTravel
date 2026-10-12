#pragma once

namespace MarkerSelectionOverride
{
    // Prepare the MarkerClick detour. Plugin startup activates it after setup succeeds.
    bool CreateHook();
}
