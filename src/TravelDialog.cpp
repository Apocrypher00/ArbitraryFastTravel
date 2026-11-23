#include "PCH.h"
#include "TravelDialog.h"
#include "GameText.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace
{
    // Native responses are Yes=1, No=2, and Place Marker=3.
    constexpr std::uint8_t buttonPressOffset   = 1;
    constexpr std::uint8_t yesResponse         = 1;
    constexpr std::uint8_t noResponse          = 2;
    constexpr std::uint8_t placeMarkerResponse = 3;

    // Preserve vanilla travel dialog parameters; CommonLib exposes no named warning enum.
    constexpr std::int32_t warningType = 0x19;
    constexpr std::int32_t menuDepth   = 4;

    // Bits from low to high select zoom, look, movement, click, and touchpad.
    // Flat 0x07 = 0b00111 selects the first three handlers.
    // VR   0x1F = 0b11111 selects all five handlers.
    constexpr std::uint32_t allMapInputFlat = 0x07;
    constexpr std::uint32_t allMapInputVR   = 0x1F;

    namespace Native
    {
        // SE52236/AE53127 checks that MapMenu is open. Yes queues fade/map close and fast travel.
        // Native No restores input; Place Marker restores input and uses the saved cursor.
        using RunTravelConfirmationFn = void (*)(RE::IMessageBoxCallback*, std::uint8_t);
        REL::Relocation<RunTravelConfirmationFn> runTravelConfirmation{};

        // SE52213/AE53100 registers or unregisters the selected map input handlers.
        // Disabling also clears the look handler state, matching vanilla's cleanup.
        using SetInputHandlersEnabledFn = void (*)(RE::MapMenu*, bool, std::uint32_t);
        REL::Relocation<SetInputHandlersEnabledFn> setInputHandlersEnabled{};

        // VR selection disables targeting while a dialog is open.
        // The native confirmation callback restores it for No and Place Marker.
        using SetMarkerInteractionEnabledFn = void (*)(bool);
        REL::Relocation<SetMarkerInteractionEnabledFn> setMarkerInteractionEnabled{};
    }

    // Native button handling expects this layout in SE 1.5.97 and the supported AE runtimes.
    // Retain the saved cursor for Place Marker and delegate all responses.
    class TravelConfirmation : public RE::IMessageBoxCallback
    {
    public:
        TravelConfirmation(RE::MapMenu* menu, float x, float y) :
            mapMenu(menu), cursorPosX(x), cursorPosY(y)
        {
            // This base-class field is padding between the reference count and map pointer.
            // Initialize it; it carries no travel decision.
            pad0C = 0;
        }

        void Run(std::uint8_t button) override
        {
            const char* response = "unknown";
            if (button == yesResponse) { response = "Yes"; }
            else if (button == noResponse) { response = "No"; }
            else if (button == placeMarkerResponse) { response = "Place Marker"; }

            // This records the response passed to native handling, not successful arrival.
            SKSE::log::info("Travel confirmation response: {} (native value={}).", response, button);
            Native::runTravelConfirmation(this, button);
        }

        RE::MapMenu* mapMenu; // 10
        float cursorPosX;     // 18: saved screen position for Place Marker
        float cursorPosY;     // 1C: saved screen position for Place Marker
    };
    // A compile-time size check catches accidental changes to this layout.
    // It is not independent proof of compatibility with other runtimes.
    static_assert(sizeof(TravelConfirmation) == 0x20);

    // VR's native response handler reads the saved placement point at +0x20.
    // Inherit the common response logging and native Run delegation.
    class TravelConfirmationVR final : public TravelConfirmation
    {
    public:
        TravelConfirmationVR(RE::MapMenu* menu, float x, float y, const RE::NiPoint3& position) :
            TravelConfirmation(menu, x, y), worldPosition(position)
        {}

        RE::NiPoint3 worldPosition;  // 20
        std::uint32_t padding = 0;   // 2C
    };
    static_assert(sizeof(TravelConfirmationVR) == 0x30);

    // VR eligibility supplies refusal text to its caller. This one-button callback
    // restores the same controls and targeting state as the native refusal dialog.
    class RefusalConfirmationVR final : public RE::IMessageBoxCallback
    {
    public:
        explicit RefusalConfirmationVR(RE::MapMenu* menu) : mapMenu(menu) { pad0C = 0; }
        void Run(std::uint8_t) override
        {
            if (RE::UI::GetSingleton()->IsMenuOpen(RE::MapMenu::MENU_NAME)) {
                TravelDialog::RestoreMapInput(mapMenu);
            }
        }
        RE::MapMenu* mapMenu;
    };

    bool FormatPrompt(const char* prompt, const char* label, RE::BSString& message)
    {
        // Format the engine prompt using its %s placeholder.
        // Measure first, then include space for the terminating zero character.
        const int length = std::snprintf(nullptr, 0, prompt, label);
        if (length < 0) {
            SKSE::log::error("Could not measure the travel confirmation text.");
            return false;
        }

        std::vector<char> text(static_cast<std::size_t>(length) + 1);
        // Do not show partial text if formatting fails or differs from the measured length.
        if (std::snprintf(text.data(), text.size(), prompt, label) != length) {
            SKSE::log::error("Could not format the complete travel confirmation text.");
            return false;
        }
        message = text.data();
        return true;
    }

}  // namespace

namespace TravelDialog
{
    void ResolveNativeFunctions()
    {
        if (REL::Module::IsVR()) {
            Native::setMarkerInteractionEnabled = REL::Offset(0x91BFF0);
        }
        Native::runTravelConfirmation   = REL::RelocationID(52236, 53127);
        Native::setInputHandlersEnabled = REL::RelocationID(52213, 53100);
    }

    void SuspendMapInput(RE::MapMenu* menu)
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

    void RestoreMapInput(RE::MapMenu* menu)
    {
        // Pair with SuspendMapInput when cancelling a locked dialog.
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

    bool ShowRefusalVR(RE::MapMenu* menu, const char* reason, const char* label)
    {
        // Use the native VR refusal dialog's localized button and formatting.
        REL::Relocation<const char**> buttonText{ REL::Offset(0x1EA27B8) };
        if (!reason || !*buttonText) { return false; }
        RE::BSString message;
        if (!FormatPrompt(reason, label ? GameText::WithoutIDPrefix(label) : "", message)) { return false; }
        auto* storage = RE::malloc<RefusalConfirmationVR>();
        if (!storage) { return false; }
        RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(std::construct_at(storage, menu));
        RE::BSTArray<RE::BSString> buttons;
        buttons.emplace_back(*buttonText);
        if (!RE::MessageBoxMenu::Create(message, callback, buttonPressOffset, warningType, 5, buttons)) {
            SKSE::log::warn("Native message-box creation declined travel refusal.");
            return false;
        }
        SuspendMapInput(menu);
        return true;
    }

    // Dialog ownership, localized text, and native button handling live here.
    bool Show(RE::MapMenu* menu, const RE::MapMenuMarker& marker, const RE::NiPoint3* placementPosition)
    {
        if (REL::Module::IsVR() && !placementPosition) {
            SKSE::log::error("VR travel confirmation requires the saved world placement position.");
            return false;
        }

        // Save the screen cursor for native Place Marker handling.
        auto* cursor = RE::MenuCursor::GetSingleton();

        // Use the same localized text as the native three-button dialog.
        const auto* yes         = GameText::Yes();
        const auto* no          = GameText::No();
        const auto* prompt      = GameText::FastTravelPrompt();
        const auto* placeMarker = GameText::PlaceMarker();

        // Match native display-name selection: a location name if present, otherwise the marker label.
        const auto* label = marker.fullName ? marker.fullName->fullName.c_str() : marker.customMarker;
        if (!cursor || !yes || !no || !prompt || !placeMarker || !label) {
            SKSE::log::error("Travel confirmation data is unavailable.");
            return false;
        }

        // Keep engine label metadata out of the visible confirmation text.
        label = GameText::WithoutIDPrefix(label);

        // BSString owns the formatted prompt after the temporary buffer is released.
        RE::BSString message;
        if (!FormatPrompt(prompt, label, message)) { return false; }

        // Preserve native ordering and response values for all three buttons.
        RE::BSTArray<RE::BSString> buttons;
        buttons.emplace_back(yes);
        buttons.emplace_back(no);
        buttons.emplace_back(placeMarker);

        // Allocate with Skyrim's heap, then construct our C++ object there.
        // construct_at initializes an object in existing storage; it does not allocate a second object.
        // Coordinates are screen positions, separate from the selected travel destination.
        TravelConfirmation* nativeLayout = nullptr;
        const auto& position = cursor->GetRuntimeData();
        if (REL::Module::IsVR()) {
            auto* storage = RE::malloc<TravelConfirmationVR>();
            if (storage) {
                nativeLayout = std::construct_at(storage, menu, position.cursorPosX, position.cursorPosY, *placementPosition);
            }
        } else {
            auto* storage = RE::malloc<TravelConfirmation>();
            if (storage) {
                nativeLayout = std::construct_at(storage, menu, position.cursorPosX, position.cursorPosY);
            }
        }
        if (!nativeLayout) {
            SKSE::log::error("Could not allocate the travel confirmation callback.");
            return false;
        }

        // The smart pointer acquires one intrusive reference.
        // Native Create retains its own reference and copies the message/buttons.
        // Our local owners may therefore go away safely when this function returns.
        // On failure, releasing this local pointer destroys the wrapper.
        RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(nativeLayout);

        // The engine can decline to create the dialog.
        // Unlike blindly suspending input, the failure path leaves controls alone and lets MarkerSelection clear the pending destination handle.
        if (!RE::MessageBoxMenu::Create(message, callback, buttonPressOffset, warningType, REL::Module::IsVR() ? 5 : menuDepth, buttons)) {
            SKSE::log::warn("Native message-box creation declined travel confirmation.");
            return false;
        }

        // Lock pan/zoom/look only after successful dialog creation.
        // Native No and Place Marker restore input; Yes closes the map through travel.
        SuspendMapInput(menu);

        return true;
    }

}
