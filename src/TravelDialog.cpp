#include "PCH.h"
#include "TravelDialog.h"
#include "GameText.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace
{
    // Native responses are Yes=1, No=2, and Place Marker=3.
    constexpr std::uint8_t buttonPressOffset = 1;

    // Preserve vanilla travel dialog parameters; CommonLib exposes no named warning enum.
    constexpr std::int32_t warningType = 0x19;
    constexpr std::int32_t menuDepth = 4;

    // The native input helper uses bits 1 (zoom), 2 (look), and 4 (movement).
    constexpr std::uint32_t allMapInput = 7;

    namespace Native
    {
        // Resolve each binding on first use, after SKSE/CommonLib initialization.
        // AE53127 checks that MapMenu is open. Yes queues fade/map close and fast travel.
        // Native No restores input; Place Marker restores input and uses the saved cursor.
        void RunTravelConfirmation(RE::IMessageBoxCallback* callback, std::uint8_t button)
        {
            using Function = void (*)(RE::IMessageBoxCallback*, std::uint8_t);
            static REL::Relocation<Function> function{REL::ID(53127)};
            function(callback, button);
        }

        // AE53100 registers or unregisters the selected map input handlers.
        // Disabling also clears the look handler state, matching vanilla's cleanup.
        void SetInputHandlersEnabled(RE::MapMenu* menu, bool enabled, std::uint32_t mask)
        {
            using Function = void (*)(RE::MapMenu*, bool, std::uint32_t);
            static REL::Relocation<Function> function{REL::ID(53100)};
            function(menu, enabled, mask);
        }
    }

    // Native button handling expects this verified 1.6.1170 callback layout.
    // Retain the saved cursor for Place Marker and delegate all responses.
    class TravelConfirmation final : public RE::IMessageBoxCallback
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
            SKSE::log::info("Travel confirmation response: {} (Yes=1, No=2, Place Marker=3).", button);
            Native::RunTravelConfirmation(this, button);
        }

        RE::MapMenu* mapMenu; // 10
        float cursorPosX;     // 18: saved screen position for Place Marker
        float cursorPosY;     // 1C: saved screen position for Place Marker
    };
    // A compile-time size check catches accidental changes to this layout.
    // It is not independent proof of compatibility with other runtimes.
    static_assert(sizeof(TravelConfirmation) == 0x20);

    bool FormatPrompt(const char* prompt, const char* label, RE::BSString& message)
    {
        // Format the engine prompt using its %s placeholder.
        // Measure first, then include space for the terminating zero character.
        const int length = std::snprintf(nullptr, 0, prompt, label);
        if (length < 0) { return false; }

        std::vector<char> text(static_cast<std::size_t>(length) + 1);
        std::snprintf(text.data(), text.size(), prompt, label);
        message = text.data();
        return true;
    }

}  // namespace

namespace TravelDialog
{
    void SuspendMapInput(RE::MapMenu* menu)
    {
        // Use only after a dialog was successfully queued, if full locking is desired.
        Native::SetInputHandlersEnabled(menu, false, allMapInput);
        menu->GetRuntimeData2()->controlsReady = false;
    }

    void RestoreMapInput(RE::MapMenu* menu)
    {
        // Pair with SuspendMapInput when cancelling a locked dialog.
        // Yes closes the map through native travel instead.
        // The caller must ensure it is open.
        Native::SetInputHandlersEnabled(menu, true, allMapInput);
        menu->GetRuntimeData2()->controlsReady = true;
    }

    // Dialog ownership, localized text, and the native Yes adapter live here.
    bool Show(RE::MapMenu* menu, const RE::MapMenuMarker& marker)
    {
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
            SKSE::log::error("Custom travel confirmation data is unavailable.");
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
        auto* storage = RE::malloc<TravelConfirmation>();
        if (!storage) { return false; }
        const auto& position = cursor->GetRuntimeData();
        auto* nativeLayout = std::construct_at(storage, menu, position.cursorPosX, position.cursorPosY);

        // The smart pointer acquires one intrusive reference.
        // Native Create retains its own reference and copies the message/buttons.
        // Our local owners may therefore go away safely when this function returns.
        // On failure, releasing this local pointer destroys the wrapper.
        RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(nativeLayout);

        // The engine can decline to create the dialog.
        // Unlike blindly suspending input, the failure path leaves controls alone and lets Selection clear the pending destination handle.
        if (!RE::MessageBoxMenu::Create(message, callback, buttonPressOffset, warningType, menuDepth, buttons)) {
            SKSE::log::warn("Native message-box creation declined custom travel confirmation.");
            return false;
        }

        // Lock pan/zoom/look only after successful dialog creation.
        // Native No and Place Marker restore input; Yes closes the map through travel.
        SuspendMapInput(menu);

        return true;
    }

}
