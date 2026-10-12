#include "PCH.h"
#include "MarkerSelectionOverride.h"

#include "Hooks.h"
#include "TravelDialog.h"
#include <cmath>

namespace MarkerSelectionOverride
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

        // Whole-function detour boundary.
        // We do not patch individual instructions or temporarily change marker types.
        // Only type 2 takes our custom path.
        // CURRENT LIMITATION: the cruise case also takes this path today.
        // Explicit delegation to vanilla for mountPredicate && cruiseEnabled is planned;
        // It is not implemented yet.
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
            if (!TravelDialog::Show(menu, marker)) {
                selectedDestination.reset();
                return;
            }
            SKSE::log::info(
                "Opened native custom travel confirmation: handle={:08X} form={:08X}.",
                static_cast<std::uint32_t>(marker.ref), destination->GetFormID()
            );
        }
    }

    bool CreateHook()
    {
        // This feature owns the engine target and function signature. Creation
        // prepares the original trampoline but does not yet activate the detour.
        // plugin.cpp enables the prepared hooks, or resets them if setup fails.
        return Hooks::Create("custom destination MarkerClick", REL::ID(53095), &Selection, originalSelection);
    }
}
