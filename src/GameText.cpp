#include "PCH.h"
#include "GameText.h"

#include <cstring>

namespace
{
    // Resolve storage addresses at startup; read the engine's current text only when needed.
    REL::Relocation<const char**> yesText{};
    REL::Relocation<const char**> noText{};
    REL::Relocation<const char**> placeMarkerText{};
    REL::Relocation<const char**> fastTravelPromptText{};

}

namespace GameText
{
    void ResolveNativeFunctions()
    {
        // IDs are paired as SE, AE, matching native localized text storage.
        yesText              = REL::VariantID(506336, 376286, 0x1EA27D0);
        noText               = REL::VariantID(506338, 376289, 0x1EA27E8);
        placeMarkerText      = REL::VariantID(507160, 377924, 0x1EA5C98);
        fastTravelPromptText = REL::VariantID(506680, 377027, 0x1EA3EF8);
    }

    // Borrow existing engine text rather than performing translation lookups.
    const char* Yes()              { return *yesText; }
    const char* No()               { return *noText; }
    const char* PlaceMarker()      { return *placeMarkerText; }
    const char* FastTravelPrompt() { return *fastTravelPromptText; }

    const char* WithoutIDPrefix(const char* text)
    {
        // Match the native helper: recognize the prefix and skip its 13-byte field.
        // This strips metadata; it does not perform a translation lookup.
        if (std::strncmp(text, "<ID=", 4) == 0) { return text + 13; }
        return text;
    }
}
