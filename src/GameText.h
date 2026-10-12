#pragma once

namespace GameText
{
    // Borrow localized text already held by the engine; do not free these pointers.
    const char* Yes();
    const char* No();
    const char* PlaceMarker();
    const char* FastTravelPrompt();

    // Return the label after its optional native ID prefix, without copying it.
    // Input must be a valid engine-created label, including the full ID field.
    const char* WithoutIDPrefix(const char* text);
}
