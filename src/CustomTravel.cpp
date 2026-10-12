#include "PCH.h"
#include "CustomTravel.h"

#include "Hooks.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace CustomTravel
{
    namespace
    {
        // MarkerClick is registered with Scaleform, Skyrim's menu system.
        // Its single argument contains the selected marker index. The function
        // pointer below describes that callback signature, not a new travel API.
        using SelectionFn = void (*)(const RE::FxDelegateArgs&);
        // MinHook fills this with a trampoline: a callable route to the original
        // function after its entry has been redirected to Selection below.
        SelectionFn originalSelection{};

        // Verified 1.6.1170 ABI.
        // The native Run implementation reads only this callback layout; use floats rather than CommonLib's integer declarations.
        // The base class supplies engine-heap delete and intrusive reference counting.
        // This is our small C++ wrapper, not a reimplementation of the button logic.
        // We cannot simply construct CommonLib's FastTravelConfirmCallback:
        // its native methods are declared without implementations, and its saved coordinate types disagree with the executable we inspected.
        // Our fields match the native callback; Run forwards to the engine.
        // A normal C++ destructor releases our object through the inherited engine allocator.
        // We do not install or manufacture a native vtable.
        class TravelConfirmation final : public RE::IMessageBoxCallback
        {
        public:
            TravelConfirmation(RE::MapMenu* menu, float x, float y) :
                mapMenu(menu), cursorPosX(x), cursorPosY(y)
            {
                // This base-class field is padding between the reference count
                // and map pointer. Initialize it; it carries no travel decision.
                pad0C = 0;
            }

            void Run(std::uint8_t button) override
            {
                // AE53127: native FastTravelConfirmCallback::Run.
                // Native code checks that MapMenu is open. Button 1 queues travel;
                // other responses restore input, and button 3 places a marker.
                // That code reads our map pointer and saved cursor coordinates
                // at the verified offsets. We supply no arrival/teleport code.
                using RunFn = void (*)(RE::IMessageBoxCallback*, std::uint8_t);
                static REL::Relocation<RunFn> run{REL::ID(53127)};
                run(this, button);
            }

            RE::MapMenu* mapMenu;  // 10
            float cursorPosX;     // 18
            float cursorPosY;     // 1C
        };
        // A compile-time size check catches accidental changes to this layout.
        // It is not independent proof of compatibility with other runtimes.
        static_assert(sizeof(TravelConfirmation) == 0x20);

        const char* LocalizedText(std::uint64_t id)
        {
            // These IDs identify storage containing pointers to engine text.
            // Dereference the storage once to obtain the actual character string.
            // The caller uses the same localized text as vanilla, not English
            // replacements. REL::ID resolves an Address Library entry at runtime.
            return *REL::Relocation<const char**>{REL::ID(id)};
        }

        // Reproduce only the dialog setup that is embedded in vanilla MarkerClick.
        // There is no identified whole-function helper for that complete block.
        // MessageBoxMenu::Create and the callback's button behavior remain native.
        // This glue is a review target for simplification, not a separate UI design.
        bool ShowConfirmation(RE::MapMenu* menu, const RE::MapMenuMarker& marker)
        {
            // Save the screen cursor position for the native Place Marker action.
            // It is unrelated to the world coordinates used for fast travel.
            auto* cursor = RE::MenuCursor::GetSingleton();
            // Verified engine text-pointer IDs: Yes, No, Place Marker, and the
            // printf-style "Fast travel to %s?" prompt, respectively.
            const auto* yes         = LocalizedText(376286);
            const auto* no          = LocalizedText(376289);
            const auto* placeMarker = LocalizedText(377924);
            const auto* prompt      = LocalizedText(377027);
            // Type 2 carries its own Custom Destination label. Ordinary marker
            // name handling is left entirely to the original selection function.
            const auto* label = marker.customMarker;
            if (!cursor || !yes || !no || !placeMarker || !prompt || !label) {
                SKSE::log::error("Custom travel confirmation data is unavailable.");
                return false;
            }

            // Mirror the native string helper: skip a 13-byte ID prefix when
            // present. This assumes the engine-created label has native format.
            // The helper strips a prefix; it does not perform a translation lookup.
            if (std::strncmp(label, "<ID=", 4) == 0) { label += 13; }
            // Format the engine prompt using its %s placeholder. First measure
            // the required length, then allocate space including the terminating
            // zero character. BSString owns the resulting message text.
            const int length = std::snprintf(nullptr, 0, prompt, label);
            if (length < 0) { return false; }
            std::vector<char> text(static_cast<std::size_t>(length) + 1);
            std::snprintf(text.data(), text.size(), prompt, label);
            RE::BSString message(text.data());
            // Native button ordering matters: Create adds an offset of 1, so
            // indices 0/1/2 reach Run as Yes=1, No=2, Place Marker=3.
            RE::BSTArray<RE::BSString> buttons;
            buttons.emplace_back(yes);
            buttons.emplace_back(no);
            buttons.emplace_back(placeMarker);

            // Allocate with Skyrim's heap, then construct our C++ object there.
            // construct_at initializes an object in existing storage; it does not
            // allocate a second object. Both coordinates remain floats throughout.
            auto* storage = RE::malloc<TravelConfirmation>();
            if (!storage) { return false; }
            const auto& position = cursor->GetRuntimeData();
            auto* nativeLayout = std::construct_at(storage, menu, position.cursorPosX, position.cursorPosY);
            // The smart pointer acquires one intrusive reference. Native Create
            // retains its own reference and copies the message/buttons. Our local
            // owners may therefore go away safely when this function returns.
            // On failure, releasing this local pointer destroys the wrapper.
            RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(nativeLayout);
            // Preserve vanilla parameters: button offset 1, warning category
            // 0x19, menu depth 4. The engine can decline to create the dialog.
            // Unlike blindly suspending input, the failure path leaves controls
            // alone and lets Selection clear the pending destination handle.
            if (!RE::MessageBoxMenu::Create(message, callback, 1, 0x19, 4, buttons)) {
                SKSE::log::warn("Native message-box creation declined custom travel confirmation.");
                return false;
            }

            // Only after a dialog was queued, suspend the map's input handlers.
            // AE53100 is the native helper. Mask 7 = zoom(1)|look(2)|move(4).
            // Its disable branch matches the inline cleanup in vanilla MarkerClick.
            // Native Run restores handlers for No and Place Marker.
            // Yes closes the map through the normal travel sequence instead.
            using InputFn = void (*)(RE::MapMenu*, bool, std::uint32_t);
            static REL::Relocation<InputFn> setInput{REL::ID(53100)};
            setInput(menu, false, 7);
            // Mirror the additional MapMenu flag cleared by vanilla's dialog
            // block. This flag is distinct from registering the input handlers.
            menu->GetRuntimeData2()->controlsReady = false;
            return true;
        }

        // Whole-function detour boundary.
        // We do not patch individual instructions or temporarily change marker types.
        // Only type 2 takes our custom path.
        // CURRENT LIMITATION: the cruise case also takes this path today.
        // Explicit delegation to vanilla for mountPredicate && cruiseEnabled is planned;
        // it is not implemented in this comments-only patch.
        void Selection(const RE::FxDelegateArgs& args)
        {
            // FxDelegateArgs exposes the originating handler, which is MapMenu for this registered callback.
            // Reject malformed arguments before indexing the marker array; vanilla's callback assumes valid UI data.
            auto* menu = static_cast<RE::MapMenu*>(args.GetHandler());
            if (!menu || args.GetArgCount() != 1 || !args[0].IsNumber()) {
                SKSE::log::warn("Ignored invalid MarkerClick arguments.");
                return;
            }
            auto* data = menu->GetRuntimeData2();
            // Scaleform represents numbers as doubles. Ensure this is a finite,
            // nonnegative integer within the array before converting to size_t.
            // These checks are defensive glue, not fast-travel eligibility rules.
            const double index = args[0].GetNumber();
            if (!data || !std::isfinite(index) || index < 0 || std::floor(index) != index ||
                index >= static_cast<double>(data->mapMarkers.size())) {
                SKSE::log::warn("Ignored invalid MarkerClick index.");
                return;
            }
            const auto& marker = data->mapMarkers[static_cast<std::size_t>(index)];
            // Regular, quest, and player markers retain their original behavior,
            // including discovery checks. Calling the trampoline avoids recursion
            // into this detour. We replace neither their dialogs nor their checks.
            if (marker.type != 2) {
                originalSelection(args);
                return;
            }

            // Vanilla clears MapMenu.mapMarker before processing a selection.
            // This is the handle the native confirmation/travel code later reads;
            // it is not the player's persistent custom-marker field.
            auto& selectedDestination = menu->GetRuntimeData()->mapMarker;
            selectedDestination.reset();
            auto* player = RE::PlayerCharacter::GetSingleton();
            // Resolve the existing marker's engine handle to its real reference.
            // The returned NiPointer keeps that reference alive during this call.
            // Do not create a fake marker or change the reference's position.
            const auto destination = RE::TESObjectREFR::LookupByHandle(marker.ref);
            // Preserve the original custom branch's use of the player's world
            // for the border check. WMS remote-map tests passed this behavior;
            // do not substitute a different world merely on that account.
            auto* world = player ? player->GetWorldspace() : nullptr;
            if (!player || !destination || !world) {
                SKSE::log::warn("Custom destination has no resolved reference or player worldspace.");
                return;
            }

            // Native checks, not copied eligibility logic:
            // AE20558: permitted XY inside border regions, when enabled by world.
            // AE40444: player restrictions (combat, weight, disabled travel, etc.).
            // The second argument remains false, as in vanilla MarkerClick, so
            // native refusal messages are shown. Its full semantics are not named.
            // Neither check establishes terrain height; placement/travel handles it.
            using BordersFn = bool (*)(RE::TESWorldSpace*, const RE::NiPoint3*);
            using RestrictionsFn = bool (*)(RE::PlayerCharacter*, bool);
            static REL::Relocation<BordersFn> withinBorders{REL::ID(20558)};
            static REL::Relocation<RestrictionsFn> canTravel{REL::ID(40444)};
            const auto position = destination->GetPosition();
            if (!withinBorders(world, &position) || !canTravel(player, false)) {
                SKSE::log::info("Native checks rejected custom travel.");
                return;
            }

            // This is the intentional eligibility change: type 2 can reach the
            // native dialog without cruise mode or ordinary map-marker discovery
            // data. We retain the checks above, then publish the destination handle.
            // We never force the later travel-type decision or the mount predicates.
            selectedDestination = destination->GetHandle();
            // If no dialog is queued, discard the handle. Input has not been
            // suspended, so there is no callback-dependent recovery to perform.
            if (!ShowConfirmation(menu, marker)) {
                selectedDestination.reset();
                return;
            }
            SKSE::log::info("Opened native custom travel confirmation: handle={:08X} form={:08X}.",
                static_cast<std::uint32_t>(marker.ref), destination->GetFormID());
        }
    }

    bool CreateHook()
    {
        // REVIEW: this runtime guard overlaps the SKSE compatibility metadata generated from CMake's COMPATIBLE_RUNTIMES.
        // WMS uses that metadata and verified per-runtime mappings.
        // Consider removing redundant enforcement in a separate behavior patch; this patch intentionally preserves it.
        // Address Library availability alone does not prove callback layout/ABI.
        if (REL::Module::get().version() != SKSE::RUNTIME_SSE_1_6_1170) {
            SKSE::log::error("Custom travel supports only the verified 1.6.1170 runtime.");
            return false;
        }
        // This feature owns the engine target and function signature. Creation
        // prepares the original trampoline but does not yet activate the detour.
        // plugin.cpp enables the prepared hooks, or resets them if setup fails.
        return Hooks::Create("custom destination MarkerClick", REL::ID(53095), &Selection, originalSelection);
    }
}
