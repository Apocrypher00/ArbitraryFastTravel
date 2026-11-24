#include "PCH.h"
#include "TravelDialog.h"
#include "GameText.h"
#include "MapInput.h"
#include "TravelCallbacks.h"

#include <cstdio>
#include <vector>

namespace
{
    // Native responses are Yes=1, No=2, and Place Marker=3.
    constexpr std::uint8_t buttonPressOffset = 1;

    // Preserve vanilla travel dialog parameters; CommonLib exposes no named warning enum.
    constexpr std::int32_t warningType = 0x19;
    constexpr std::int32_t menuDepth   = 4;
    constexpr std::int32_t menuDepthVR = 5;

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
    bool ShowRefusalVR(RE::MapMenu* menu, const char* reason, const char* label)
    {
        // Read the same localized button text used by the native VR refusal dialog.
        const char* buttonLabel = GameText::GetSettingText("sOk");
        if (!reason || !buttonLabel) { return false; }

        // The refusal reason can contain a placeholder for the destination label.
        const char* destinationLabel = label ? GameText::WithoutIDPrefix(label) : "";
        RE::BSString message;
        if (!FormatPrompt(reason, destinationLabel, message)) { return false; }

        auto callback = TravelCallbacks::CreateRefusalCallbackVR(menu);
        if (!callback) { return false; }

        // Create retains the callback and copies the message and button text.
        RE::BSTArray<RE::BSString> buttons;
        buttons.emplace_back(buttonLabel);

        const bool queued = RE::MessageBoxMenu::Create(message, callback, buttonPressOffset, warningType, menuDepthVR, buttons);
        if (!queued) {
            SKSE::log::warn("Native message-box creation declined travel refusal.");
            return false;
        }

        // Suspend only after the dialog is queued. Its callback restores input.
        MapInput::Suspend(menu);
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

        // The native dialog uses these named game settings for its localized text.
        const auto* yes         = GameText::GetSettingText("sYes");
        const auto* no          = GameText::GetSettingText("sNo");
        const auto* prompt      = GameText::GetSettingText("sFastTravelConfirm");
        const auto* placeMarker = GameText::GetSettingText("sPlaceMarker");

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

        auto callback = TravelCallbacks::CreateTravelCallback(menu, cursor->GetRuntimeData(), placementPosition);
        if (!callback) {
            SKSE::log::error("Could not allocate the travel confirmation callback.");
            return false;
        }

        // The engine can decline to create the dialog.
        // Unlike blindly suspending input, the failure path leaves controls alone and lets MarkerSelection clear the pending destination handle.
        if (!RE::MessageBoxMenu::Create(message, callback, buttonPressOffset, warningType, REL::Module::IsVR() ? menuDepthVR : menuDepth, buttons)) {
            SKSE::log::warn("Native message-box creation declined travel confirmation.");
            return false;
        }

        // Lock pan/zoom/look only after successful dialog creation.
        // Native No and Place Marker restore input; Yes closes the map through travel.
        MapInput::Suspend(menu);

        return true;
    }

}
