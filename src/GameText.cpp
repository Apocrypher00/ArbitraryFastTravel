#include "PCH.h"
#include "GameText.h"

#include <cstring>

namespace GameText
{
    // Query when the dialog is requested, after Skyrim has loaded its game settings.
    const char* GetSettingText(const char* name)
    {
        auto* settings = RE::GameSettingCollection::GetSingleton();
        const auto* setting = settings ? settings->GetSetting(name) : nullptr;
        return setting ? setting->GetString() : nullptr;
    }

    const char* WithoutIDPrefix(const char* text)
    {
        // Match the native helper: recognize the prefix and skip its 13-byte field.
        // This strips metadata; it does not perform a translation lookup.
        if (std::strncmp(text, "<ID=", 4) == 0) { return text + 13; }
        return text;
    }
}
