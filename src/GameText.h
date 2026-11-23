#pragma once

namespace GameText
{
    // Borrow the current text from a named string game setting, or return nullptr
    // if it is unavailable. Skyrim owns the returned text.
    const char* GetSettingText(const char* name);

    // Return the label after its optional native ID prefix, without copying it.
    // Input must be a valid engine-created label, including the full ID field.
    const char* WithoutIDPrefix(const char* text);
}
