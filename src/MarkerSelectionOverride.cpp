#include "PCH.h"
#include "MarkerSelectionOverride.h"

#include "Hooks.h"
#include "TravelDialog.h"
#include <cmath>

namespace MarkerSelectionOverride
{
    namespace
    {
        // CommonLib exposes the marker type as a number rather than a named enum.
        constexpr std::uint32_t customDestinationType = 2;

        namespace Native
        {
            // Keep bindings beside their owning feature and resolve them on first use.
            // These helpers call the engine; they do not reproduce its checks.
            // AE37901 is vanilla's mount-state predicate for cruise selection.
            // It checks the current mount and a process flag; the exact flag meaning is unresolved.
            // Reuse the predicate instead of treating every mounted player as cruising.
            bool HasCruiseMountState(RE::PlayerCharacter* player)
            {
                using Function = bool (*)(RE::PlayerCharacter*);
                static REL::Relocation<Function> function{REL::ID(37901)};
                return function(player);
            }

            // AE40728 reads the cruise setting used by vanilla MarkerClick.
            // Enabled/disabled dragon tests established its role in that combined condition.
            bool IsCruiseEnabled()
            {
                using Function = bool (*)();
                static REL::Relocation<Function> function{REL::ID(40728)};
                return function();
            }

            // AE20558 checks destination XY against border regions when enabled by the world.
            // It does not establish terrain height.
            bool IsPositionWithinBorders(RE::TESWorldSpace* world, const RE::NiPoint3* position)
            {
                using Function = bool (*)(RE::TESWorldSpace*, const RE::NiPoint3*);
                static REL::Relocation<Function> function{REL::ID(20558)};
                return function(world, position);
            }

            // AE40444 applies player restrictions such as combat, weight, and disabled travel.
            // Vanilla MarkerClick passes false and native refusal messages are shown.
            // The mode argument's full semantics are not named; this is not a height check.
            bool CanFastTravel(RE::PlayerCharacter* player, bool mode)
            {
                using Function = bool (*)(RE::PlayerCharacter*, bool);
                static REL::Relocation<Function> function{REL::ID(40444)};
                return function(player, mode);
            }
        }

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
        // Vanilla mount cruising retains the entire original selection path.
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

            // Regular, quest, and player markers retain their original behavior, including discovery checks.
            // Calling the trampoline avoids recursion into this detour.
            // We replace neither their dialogs nor their checks.
            if (marker.type != customDestinationType) {
                originalSelection(args);
                return;
            }

            // Preserve vanilla cruising before changing the selected destination or opening our dialog.
            // Mount state alone is insufficient: cruise must also be enabled.
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && Native::HasCruiseMountState(player) && Native::IsCruiseEnabled()) {
                SKSE::log::info("Delegated custom destination selection to native mount cruising.");
                originalSelection(args);
                return;
            }

            // Vanilla clears MapMenu.mapMarker before processing a selection.
            // This is the handle the native confirmation/travel code later reads;
            // it is not the player's persistent custom-marker field.
            auto& selectedDestination = menu->GetRuntimeData()->mapMarker;
            selectedDestination.reset();

            // Resolve the existing marker's engine handle to its real reference.
            // The returned NiPointer keeps that reference alive during this call.
            // Do not create a fake marker or change the reference's position.
            const auto destination = RE::TESObjectREFR::LookupByHandle(marker.ref);

            // Preserve the original custom branch's use of the player's world for the border check.
            // WMS remote-map tests passed this behavior; do not substitute a different world merely on that account.
            auto* world = player ? player->GetWorldspace() : nullptr;
            if (!player || !destination || !world) {
                SKSE::log::warn("Custom destination has no resolved reference or player worldspace.");
                return;
            }

            // Preserve the native border and player restriction checks.
            const auto position = destination->GetPosition();
            if (!Native::IsPositionWithinBorders(world, &position) || !Native::CanFastTravel(player, false)) {
                SKSE::log::info("Native checks rejected custom travel.");
                return;
            }

            // This is the intentional eligibility change:
            // type 2 can reach the native dialog without cruise mode or ordinary map-marker discovery data.
            // We retain the checks above, then publish the destination handle.
            // We never force the later travel-type decision or the mount predicates.
            selectedDestination = destination->GetHandle();

            // If no dialog is queued, discard the handle.
            // Input has not been suspended, so there is no callback-dependent recovery to perform.
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
        // This feature owns the engine target and function signature.
        // Creation prepares the original trampoline but does not yet activate the detour.
        // plugin.cpp enables the prepared hooks, or resets them if setup fails.
        return Hooks::Create("custom destination MarkerClick", REL::ID(53095), &Selection, originalSelection);
    }
}
