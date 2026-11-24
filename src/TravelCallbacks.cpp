#include "PCH.h"
#include "TravelCallbacks.h"
#include "MapInput.h"

#include <memory>

namespace
{
    // Native responses are Yes=1, No=2, and Place Marker=3.
    constexpr std::uint8_t yesResponse         = 1;
    constexpr std::uint8_t noResponse          = 2;
    constexpr std::uint8_t placeMarkerResponse = 3;

    namespace Native
    {
        // SE52236/AE53127 checks that MapMenu is open. Yes queues fade/map close and fast travel.
        // Native No restores input; Place Marker restores input and uses the saved cursor.
        using RunTravelConfirmationFn = void (*)(RE::IMessageBoxCallback*, std::uint8_t);
        REL::Relocation<RunTravelConfirmationFn> runTravelConfirmation{};

    }

    // Native button handling expects this layout in SE 1.5.97 and the supported AE runtimes.
    // Retain the saved cursor for Place Marker and delegate all responses.
    class TravelConfirmation : public RE::IMessageBoxCallback
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
            const char* response = "unknown";
            if (button == yesResponse) { response = "Yes"; }
            else if (button == noResponse) { response = "No"; }
            else if (button == placeMarkerResponse) { response = "Place Marker"; }

            // This records the response passed to native handling, not successful arrival.
            SKSE::log::info("Travel confirmation response: {} (native value={}).", response, button);
            Native::runTravelConfirmation(this, button);
        }

        RE::MapMenu* mapMenu; // 10
        float cursorPosX;     // 18: saved screen position for Place Marker
        float cursorPosY;     // 1C: saved screen position for Place Marker
    };
    // A compile-time size check catches accidental changes to this layout.
    // It is not independent proof of compatibility with other runtimes.
    static_assert(sizeof(TravelConfirmation) == 0x20);

    // VR's native response handler reads the saved placement point at +0x20.
    // Inherit the common response logging and native Run delegation.
    class TravelConfirmationVR final : public TravelConfirmation
    {
    public:
        TravelConfirmationVR(RE::MapMenu* menu, float x, float y, const RE::NiPoint3& position) :
            TravelConfirmation(menu, x, y), worldPosition(position)
        {}

        RE::NiPoint3 worldPosition; // 20
        std::uint32_t padding = 0;  // 2C
    };
    static_assert(sizeof(TravelConfirmationVR) == 0x30);

    // VR eligibility supplies refusal text to its caller.
    // This one-button callback restores the same controls and targeting state as the native refusal dialog.
    class RefusalConfirmationVR final : public RE::IMessageBoxCallback
    {
    public:
        explicit RefusalConfirmationVR(RE::MapMenu* menu) : mapMenu(menu) { pad0C = 0; }
        void Run(std::uint8_t) override
        {
            if (RE::UI::GetSingleton()->IsMenuOpen(RE::MapMenu::MENU_NAME)) {
                MapInput::Restore(mapMenu);
            }
        }
        RE::MapMenu* mapMenu;
    };

}  // namespace

namespace TravelCallbacks
{
    void ResolveNativeFunctions()
    {
        Native::runTravelConfirmation = REL::RelocationID(52236, 53127);
    }

    RE::BSTSmartPointer<RE::IMessageBoxCallback> CreateTravelCallback(RE::MapMenu* menu, const RE::MenuCursor::RUNTIME_DATA& cursorPosition, const RE::NiPoint3* placementPosition)
    {
        // Construct the runtime-specific layout on Skyrim's heap.
        // Show validates the VR placement point before calling this helper.
        TravelConfirmation* nativeLayout = nullptr;
        if (REL::Module::IsVR()) {
            auto* storage = RE::malloc<TravelConfirmationVR>();
            if (storage) {
                nativeLayout = std::construct_at(storage, menu, cursorPosition.cursorPosX, cursorPosition.cursorPosY, *placementPosition);
            }
        } else {
            auto* storage = RE::malloc<TravelConfirmation>();
            if (storage) {
                nativeLayout = std::construct_at(storage, menu, cursorPosition.cursorPosX, cursorPosition.cursorPosY);
            }
        }

        // An empty pointer reports allocation failure.
        // Otherwise this acquires one reference; the native dialog retains its own reference when queued.
        return RE::BSTSmartPointer<RE::IMessageBoxCallback>(nativeLayout);
    }

    RE::BSTSmartPointer<RE::IMessageBoxCallback> CreateRefusalCallbackVR(RE::MapMenu* menu)
    {
        auto* storage = RE::malloc<RefusalConfirmationVR>();
        if (!storage) { return {}; }

        auto* refusalCallback = std::construct_at(storage, menu);
        return RE::BSTSmartPointer<RE::IMessageBoxCallback>(refusalCallback);
    }
}
