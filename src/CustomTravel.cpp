#include "PCH.h"
#include "CustomTravel.h"

#include <MinHook.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace CustomTravel
{
    namespace
    {
        using SelectionFn = void (*)(const RE::FxDelegateArgs&);
        SelectionFn originalSelection{};

        // Verified 1.6.1170 ABI. The native Run implementation reads only this
        // callback layout; use floats rather than CommonLib's integer declarations.
        // The base class supplies engine-heap delete and intrusive reference counting.
        class TravelConfirmation final : public RE::IMessageBoxCallback
        {
        public:
            TravelConfirmation(RE::MapMenu* menu, float x, float y) :
                mapMenu(menu), cursorPosX(x), cursorPosY(y)
            {
                pad0C = 0;
            }

            void Run(std::uint8_t button) override
            {
                using RunFn = void (*)(RE::IMessageBoxCallback*, std::uint8_t);
                static REL::Relocation<RunFn> run{REL::ID(53127)};
                run(this, button);
            }

            RE::MapMenu* mapMenu;  // 10
            float cursorPosX;     // 18
            float cursorPosY;     // 1C
        };
        static_assert(sizeof(TravelConfirmation) == 0x20);

        const char* LocalizedText(std::uint64_t id)
        {
            return *REL::Relocation<const char**>{REL::ID(id)};
        }

        bool ShowConfirmation(RE::MapMenu* menu, const RE::MapMenuMarker& marker)
        {
            auto* cursor = RE::MenuCursor::GetSingleton();
            const auto* yes = LocalizedText(376286);
            const auto* no = LocalizedText(376289);
            const auto* placeMarker = LocalizedText(377924);
            const auto* prompt = LocalizedText(377027);
            const auto* label = marker.customMarker;
            if (!cursor || !yes || !no || !placeMarker || !prompt || !label) {
                SKSE::log::error("Custom travel confirmation data is unavailable.");
                return false;
            }

            if (std::strncmp(label, "<ID=", 4) == 0) { label += 13; }
            const int length = std::snprintf(nullptr, 0, prompt, label);
            if (length < 0) { return false; }
            std::vector<char> text(static_cast<std::size_t>(length) + 1);
            std::snprintf(text.data(), text.size(), prompt, label);
            RE::BSString message(text.data());
            RE::BSTArray<RE::BSString> buttons;
            buttons.emplace_back(yes);
            buttons.emplace_back(no);
            buttons.emplace_back(placeMarker);

            auto* storage = RE::malloc<TravelConfirmation>();
            if (!storage) { return false; }
            const auto& position = cursor->GetRuntimeData();
            auto* nativeLayout = std::construct_at(storage, menu, position.cursorPosX, position.cursorPosY);
            RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(nativeLayout);
            if (!RE::MessageBoxMenu::Create(message, callback, 1, 0x19, 4, buttons)) {
                SKSE::log::warn("Native message-box creation declined custom travel confirmation.");
                return false;
            }

            using InputFn = void (*)(RE::MapMenu*, bool, std::uint32_t);
            static REL::Relocation<InputFn> setInput{REL::ID(53100)};
            setInput(menu, false, 7);
            menu->GetRuntimeData2()->controlsReady = false;
            return true;
        }

        void Selection(const RE::FxDelegateArgs& args)
        {
            auto* menu = static_cast<RE::MapMenu*>(args.GetHandler());
            if (!menu || args.GetArgCount() != 1 || !args[0].IsNumber()) {
                SKSE::log::warn("Ignored invalid MarkerClick arguments.");
                return;
            }
            auto* data = menu->GetRuntimeData2();
            const double index = args[0].GetNumber();
            if (!data || !std::isfinite(index) || index < 0 || std::floor(index) != index ||
                index >= static_cast<double>(data->mapMarkers.size())) {
                SKSE::log::warn("Ignored invalid MarkerClick index.");
                return;
            }
            const auto& marker = data->mapMarkers[static_cast<std::size_t>(index)];
            if (marker.type != 2) {
                originalSelection(args);
                return;
            }

            auto& selectedDestination = menu->GetRuntimeData()->mapMarker;
            selectedDestination.reset();
            auto* player = RE::PlayerCharacter::GetSingleton();
            const auto destination = RE::TESObjectREFR::LookupByHandle(marker.ref);
            auto* world = player ? player->GetWorldspace() : nullptr;
            if (!player || !destination || !world) {
                SKSE::log::warn("Custom destination has no resolved reference or player worldspace.");
                return;
            }

            using BordersFn = bool (*)(RE::TESWorldSpace*, const RE::NiPoint3*);
            using RestrictionsFn = bool (*)(RE::PlayerCharacter*, bool);
            static REL::Relocation<BordersFn> withinBorders{REL::ID(20558)};
            static REL::Relocation<RestrictionsFn> canTravel{REL::ID(40444)};
            const auto position = destination->GetPosition();
            if (!withinBorders(world, &position) || !canTravel(player, false)) {
                SKSE::log::info("Native checks rejected custom travel.");
                return;
            }

            selectedDestination = destination->GetHandle();
            if (!ShowConfirmation(menu, marker)) {
                selectedDestination.reset();
                return;
            }
            SKSE::log::info("Opened native custom travel confirmation: handle={:08X} form={:08X}.",
                static_cast<std::uint32_t>(marker.ref), destination->GetFormID());
        }
    }

    bool Install()
    {
        if (REL::Module::get().version() != SKSE::RUNTIME_SSE_1_6_1170) {
            SKSE::log::error("Custom travel supports only the verified 1.6.1170 runtime.");
            return false;
        }
        const auto initialized = MH_Initialize();
        if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) {
            SKSE::log::error("MinHook initialization failed: {}", MH_StatusToString(initialized));
            return false;
        }
        auto* target = reinterpret_cast<void*>(REL::Relocation<std::uintptr_t>{REL::ID(53095)}.address());
        const auto created = MH_CreateHook(target, reinterpret_cast<void*>(&Selection),
            reinterpret_cast<void**>(&originalSelection));
        if (created != MH_OK) {
            SKSE::log::error("MarkerClick detour failed: {}", MH_StatusToString(created));
            return false;
        }
        const auto enabled = MH_EnableHook(target);
        if (enabled != MH_OK) {
            MH_RemoveHook(target);
            SKSE::log::error("MarkerClick detour enable failed: {}", MH_StatusToString(enabled));
            return false;
        }
        SKSE::log::info("Installed custom destination MarkerClick detour.");
        return true;
    }
}
