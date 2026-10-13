#include "PCH.h"
#include "GameText.h"

#include <cstring>

namespace
{
    const char* ReadTextPointer(REL::RelocationID id)
    {
        // IDs are paired as SE, AE, matching the native localized text storage.
        // This reads existing text; it does not translate an arbitrary string.
        // CommonLib's settings/translator APIs are different entry points;
        // equivalence to these native dialog pointers has not been established.
        return *REL::Relocation<const char**>{id};
    }
}

namespace GameText
{
    const char* Yes() { return ReadTextPointer(REL::RelocationID(506336, 376286)); }
    const char* No() { return ReadTextPointer(REL::RelocationID(506338, 376289)); }
    const char* PlaceMarker() { return ReadTextPointer(REL::RelocationID(507160, 377924)); }
    const char* FastTravelPrompt() { return ReadTextPointer(REL::RelocationID(506680, 377027)); }

    const char* WithoutIDPrefix(const char* text)
    {
        // Match the native helper: recognize the prefix and skip its 13-byte field.
        // This strips metadata; it does not perform a translation lookup.
        if (std::strncmp(text, "<ID=", 4) == 0) { return text + 13; }
        return text;
    }
}
