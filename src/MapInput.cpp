#include "PCH.h"
#include "MapInput.h"

namespace
{
    // Bits from low to high select zoom, look, movement, click, and touchpad.
    // Flat 0x07 = 0b00111 selects the first three handlers.
    // VR   0x1F = 0b11111 selects all five handlers.
    constexpr std::uint32_t allMapInputFlat = 0x07;
    constexpr std::uint32_t allMapInputVR   = 0x1F;

    namespace Native
    {
        // SE52213/AE53100 registers or unregisters the selected map input handlers.
        // Disabling also clears the look handler state, matching vanilla's cleanup.
        using SetInputHandlersEnabledFn = void (*)(RE::MapMenu*, bool, std::uint32_t);
        REL::Relocation<SetInputHandlersEnabledFn> setInputHandlersEnabled{};

        // VR selection disables targeting while a dialog is open.
        // The native confirmation callback restores it for No and Place Marker.
        using SetMarkerInteractionEnabledFn = void (*)(bool);
        REL::Relocation<SetMarkerInteractionEnabledFn> setMarkerInteractionEnabled{};
    }
}

namespace MapInput
{
    void ResolveNativeFunctions()
    {
        Native::setInputHandlersEnabled = REL::RelocationID(52213, 53100);
        if (REL::Module::IsVR()) {
            Native::setMarkerInteractionEnabled = REL::Offset(0x91BFF0);
        }
    }

    void Suspend(RE::MapMenu* menu)
    {
        // Use only after a dialog was successfully queued, if full locking is desired.
        Native::setInputHandlersEnabled(menu, false, REL::Module::IsVR() ? allMapInputVR : allMapInputFlat);
        if (REL::Module::IsVR()) {
            auto* data = menu->GetVRRuntimeData2();
            data->controlsReady = false;
            if (data->crosshairParent) {
                data->crosshairParent->GetFlags().set(RE::NiAVObject::Flag::kHidden);
            }
            Native::setMarkerInteractionEnabled(false);
        } else {
            menu->GetRuntimeData2()->controlsReady = false;
        }
    }

    void Restore(RE::MapMenu* menu)
    {
        // Pair with Suspend when cancelling a locked dialog.
        // Yes closes the map through native travel instead.
        // The caller must ensure it is open.
        Native::setInputHandlersEnabled(menu, true, REL::Module::IsVR() ? allMapInputVR : allMapInputFlat);
        if (REL::Module::IsVR()) {
            auto* data = menu->GetVRRuntimeData2();
            data->controlsReady = true;
            if (data->crosshairParent) {
                data->crosshairParent->GetFlags().reset(RE::NiAVObject::Flag::kHidden);
            }
            Native::setMarkerInteractionEnabled(true);
        } else {
            menu->GetRuntimeData2()->controlsReady = true;
        }
    }

}
