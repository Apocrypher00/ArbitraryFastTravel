#include "PCH.h"
#include "GameText.h"

#include <cstring>

namespace
{
    const char* ReadTextPointer(std::uint64_t id)
    {
        // Verified engine storage containing a localized text pointer.
        // This reads existing text; it does not translate an arbitrary string.
        // CommonLib's settings/translator APIs are different entry points;
        // equivalence to these native dialog pointers has not been established.
        return *REL::Relocation<const char**>{REL::ID(id)};
    }
}

namespace GameText
{
    const char* Yes() { return ReadTextPointer(376286); }
    const char* No() { return ReadTextPointer(376289); }
    const char* PlaceMarker() { return ReadTextPointer(377924); }
    const char* FastTravelPrompt() { return ReadTextPointer(377027); }

    const char* WithoutIDPrefix(const char* text)
    {
        // Match the native helper: recognize the prefix and skip its 13-byte field.
        // This strips metadata; it does not perform a translation lookup.
        if (std::strncmp(text, "<ID=", 4) == 0) { return text + 13; }
        return text;
    }
}
