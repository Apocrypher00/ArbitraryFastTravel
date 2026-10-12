#include "PCH.h"
#include "TravelDialog.h"
#include "GameText.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace
{
    // Keep the native callback numbering: the first button returns 1, the second 2.
    constexpr std::uint8_t buttonPressOffset = 1;
    constexpr std::uint8_t yesResponse = buttonPressOffset;

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
        // Our current callback delegates only Yes, so neither other branch is entered.
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

    // Native Yes handling expects the verified 1.6.1170 callback layout.
    // Keep that layout and engine allocation, but delegate only acceptance.
    // No/cancel needs no input recovery because we never suspend map input.
    class TravelConfirmation final : public RE::IMessageBoxCallback
    {
    public:
        explicit TravelConfirmation(RE::MapMenu* menu) : mapMenu(menu)
        {
            // This base-class field is padding between the reference count
            // and map pointer. Initialize it; it carries no travel decision.
            pad0C = 0;
        }

        void Run(std::uint8_t button) override
        {
            // Offset 1 makes Yes=1 and No=2. Ignore cancellation and any
            // unexpected response; only Yes enters the native travel sequence.
            if (button != yesResponse) { return; }
            Native::RunTravelConfirmation(this, button);
        }

        RE::MapMenu* mapMenu;  // 10
        float cursorPosX{};   // 18: retained for ABI; Yes does not use it
        float cursorPosY{};   // 1C: retained for ABI; Yes does not use it
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
        // Verified engine text pointers: Yes, No, and the travel prompt.
        const auto* yes    = GameText::Yes();
        const auto* no     = GameText::No();
        const auto* prompt = GameText::FastTravelPrompt();

        // Type 2 carries its own Custom Destination label.
        // Ordinary marker name handling is left entirely to the original selection function.
        const auto* label = marker.customMarker;
        if (!yes || !no || !prompt || !label) {
            SKSE::log::error("Custom travel confirmation data is unavailable.");
            return false;
        }

        // Keep engine label metadata out of the visible confirmation text.
        label = GameText::WithoutIDPrefix(label);

        // BSString owns the formatted prompt after the temporary buffer is released.
        RE::BSString message;
        if (!FormatPrompt(prompt, label, message)) { return false; }

        // Native button ordering matters: Create adds an offset of 1, so indices 0/1 reach Run as Yes=1, No=2.
        RE::BSTArray<RE::BSString> buttons;
        buttons.emplace_back(yes);
        buttons.emplace_back(no);

        // Allocate with Skyrim's heap, then construct our C++ object there.
        // construct_at initializes an object in existing storage; it does not allocate a second object.
        // No saved cursor position is needed for Yes.
        auto* storage = RE::malloc<TravelConfirmation>();
        if (!storage) { return false; }
        auto* nativeLayout = std::construct_at(storage, menu);

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

        // Leave map input alone.
        // The dialog captures its own buttons, while edge panning remains available, as observed in the WMS chooser.

        return true;
    }

}
