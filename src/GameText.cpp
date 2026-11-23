#include "PCH.h"
#include "GameText.h"

#include <cstring>

namespace
{
    // Query when the dialog is requested, after Skyrim has loaded its game settings.
    const char* GetSettingText(const char* name)
    {
        auto* settings = RE::GameSettingCollection::GetSingleton();
        const auto* setting = settings ? settings->GetSetting(name) : nullptr;
        return setting ? setting->GetString() : nullptr;
    }
}

namespace GameText
{
    // Skyrim stores these localized button labels and the travel prompt as named game settings.
    // Read their current values when building the dialog.
    const char* Yes()              { return GetSettingText("sYes"); }
    const char* No()               { return GetSettingText("sNo"); }
    const char* Ok()               { return GetSettingText("sOk"); }
    const char* PlaceMarker()      { return GetSettingText("sPlaceMarker"); }
    const char* FastTravelPrompt() { return GetSettingText("sFastTravelConfirm"); }

    const char* WithoutIDPrefix(const char* text)
    {
        // Match the native helper: recognize the prefix and skip its 13-byte field.
        // This strips metadata; it does not perform a translation lookup.
        if (std::strncmp(text, "<ID=", 4) == 0) { return text + 13; }
        return text;
    }
}
