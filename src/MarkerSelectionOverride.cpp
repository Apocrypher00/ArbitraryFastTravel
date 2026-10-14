#include "PCH.h"
#include "MarkerSelectionOverride.h"

#include "Hooks.h"
#include "Config.h"
#include "TravelDialog.h"
#include <cmath>

namespace MarkerSelectionOverride
{
    namespace
    {
        // CommonLib exposes the marker type as a number rather than a named enum.
        constexpr std::uint32_t regularMarkerType = 0;
        constexpr std::uint32_t questMarkerType = 1;
        constexpr std::uint32_t customDestinationType = 2;

        // Quest-door display category verified in SE 1.5.97 and the supported AE runtimes.
        // CommonLib agrees on this value.
        constexpr std::uint32_t doorQuestDisplayType = 0x3F;

        namespace Native
        {
            // Keep bindings beside their owning feature and resolve them on first use.
            // These helpers call the engine; they do not reproduce its checks.

            // SE52190/AE53082 selects the native display category from a map-marker record.
            // Quest markers return 0x3F for teleport destinations or the current-location routing fallback.
            // Direct quest variants return 0x3E or 0x40. This classifies routing, not the objective's cell.
            std::uint32_t GetDisplayType(const RE::MapMenuMarker& marker)
            {
                using Function = std::uint32_t (*)(const RE::MapMenuMarker*);
                static REL::Relocation<Function> function{REL::RelocationID(52190, 53082)};
                return function(&marker);
            }

            // SE36877/AE37901 is vanilla's mount-state predicate for cruise selection.
            // It checks the current mount and a process flag; the exact flag meaning is unresolved.
            // Reuse the predicate instead of treating every mounted player as cruising.
            bool HasCruiseMountState(RE::PlayerCharacter* player)
            {
                using Function = bool (*)(RE::PlayerCharacter*);
                static REL::Relocation<Function> function{REL::RelocationID(36877, 37901)};
                return function(player);
            }

            // SE39641/AE40728 reads the cruise setting used by vanilla MarkerClick.
            // Enabled/disabled dragon tests established its role in that combined condition.
            bool IsCruiseEnabled()
            {
                using Function = bool (*)();
                static REL::Relocation<Function> function{REL::RelocationID(39641, 40728)};
                return function();
            }

            // SE20109/AE20558 checks destination XY against border regions when enabled by the world.
            // It does not establish terrain height and requires a non-null worldspace.
            // Vanilla uses this in its custom dragon-cruising path, which we delegate to originalSelection.
            // Retained for reference; AFT's ordinary fast-travel path does not call this helper.
            [[maybe_unused]] bool IsPositionWithinBorders(RE::TESWorldSpace* world, const RE::NiPoint3* position)
            {
                using Function = bool (*)(RE::TESWorldSpace*, const RE::NiPoint3*);
                static REL::Relocation<Function> function{REL::RelocationID(20109, 20558)};
                return function(world, position);
            }

            // SE39372/AE40444 applies player restrictions such as combat, weight, and disabled travel.
            // Vanilla MarkerClick passes false and native refusal messages are shown.
            // The mode argument's full semantics are not named; this is not a height check.
            bool CanFastTravel(RE::PlayerCharacter* player, bool mode)
            {
                using Function = bool (*)(RE::PlayerCharacter*, bool);
                static REL::Relocation<Function> function{REL::RelocationID(39372, 40444)};
                return function(player, mode);
            }
        }

        void LogQuestDestination(const RE::MapMenuMarker& marker, RE::TESObjectREFR* reference)
        {
            // Read-only diagnostics: distinguish a displayed exterior door from an outdoor target.
            const auto* base    = reference->GetBaseObject();
            const auto* cell    = reference->GetParentCell();
            const auto* world   = reference->GetWorldspace();
            const auto position = reference->GetPosition();
            const auto* name    = reference->GetName();
            SKSE::log::debug(
                "Quest destination: ref={:08X} name={} base={:08X} isDoor={} cell={:08X} interior={} world={:08X} markerDoor={} xyz=({:.2f}, {:.2f}, {:.2f}).",
                reference->GetFormID(), name ? name : "", base ? base->GetFormID() : 0,
                base && base->Is(RE::FormType::Door), cell ? cell->GetFormID() : 0,
                cell && cell->IsInteriorCell(), world ? world->GetFormID() : 0, marker.door,
                position.x, position.y, position.z
            );

            const auto* teleport = reference->extraList.GetByType<RE::ExtraTeleport>();
            if (!teleport || !teleport->teleportData) { return; }
            const auto& data = *teleport->teleportData;
            const auto linked = data.linkedDoor.get();
            SKSE::log::debug(
                "Door teleport: linkedHandle={:08X} linkedRef={:08X} arrival=({:.2f}, {:.2f}, {:.2f}) rotation=({:.3f}, {:.3f}, {:.3f}).",
                data.linkedDoor.native_handle(), linked ? linked->GetFormID() : 0,
                data.position.x, data.position.y, data.position.z,
                data.rotation.x, data.rotation.y, data.rotation.z
            );

            // The selected door's arrival may point indoors. Inspect its counterpart as well.
            // Lookup holds existing references; this does not force cells to load or move anything.
            if (!linked) { return; }
            const auto* linkedCell = linked->GetParentCell();
            const auto* counterpart = linked->extraList.GetByType<RE::ExtraTeleport>();
            SKSE::log::debug(
                "Linked door: ref={:08X} cell={:08X} interior={} hasTeleport={}.",
                linked->GetFormID(), linkedCell ? linkedCell->GetFormID() : 0,
                linkedCell && linkedCell->IsInteriorCell(), counterpart && counterpart->teleportData
            );
            if (counterpart && counterpart->teleportData) {
                const auto& arrival = *counterpart->teleportData;
                SKSE::log::debug(
                    "Linked door teleport: returnHandle={:08X} arrival=({:.2f}, {:.2f}, {:.2f}) rotation=({:.3f}, {:.3f}, {:.3f}).",
                    arrival.linkedDoor.native_handle(),
                    arrival.position.x, arrival.position.y, arrival.position.z,
                    arrival.rotation.x, arrival.rotation.y, arrival.rotation.z
                );
            }
        }

        bool IsDiscoveredMarker(const RE::MapMenuMarker& marker)
        {
            // Use CommonLib's named native eligibility flag without changing marker data.
            const auto reference = RE::TESObjectREFR::LookupByHandle(marker.ref);
            const auto* extra = reference ? reference->extraList.GetByType<RE::ExtraMapMarker>() : nullptr;
            return extra && extra->mapData && extra->mapData->flags.any(RE::MapMarkerData::Flag::kCanTravelTo);
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
        // Expand regular undiscovered, quest, and custom destination eligibility.
        // Vanilla mount cruising retains the entire original selection path.
        void Selection(const RE::FxDelegateArgs& args)
        {
            // Start a new log group even for clicks delegated entirely to vanilla.
            SKSE::log::debug("MarkerClick: begin (arguments={}).", args.GetArgCount());

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
            SKSE::log::debug(
                "MarkerClick: selected index={} type={} handle={:08X}.",
                index, marker.type, static_cast<std::uint32_t>(marker.ref)
            );

            // Leave player markers and any unknown categories entirely native.
            if (marker.type != regularMarkerType && marker.type != questMarkerType && marker.type != customDestinationType) {
                SKSE::log::debug("MarkerClick: vanilla handoff (marker category is not overridden).");
                originalSelection(args);
                return;
            }

            // Discovered locations retain their complete vanilla path regardless of configuration.
            if (marker.type == regularMarkerType && IsDiscoveredMarker(marker)) {
                SKSE::log::debug("MarkerClick: vanilla handoff (discovered location).");
                originalSelection(args);
                return;
            }

            // Disabled categories use the complete original selection function.
            // Regular markers reaching this point are undiscovered.
            // This happens before destination changes, cruise checks, or our dialog.
            if (
                (marker.type == customDestinationType && !Config::GetAllowCustomDestination()) ||
                (marker.type == questMarkerType && !Config::GetAllowDirectQuestMarkers() && !Config::GetAllowDoorQuestMarkers()) ||
                (marker.type == regularMarkerType && !Config::GetAllowUndiscoveredLocations())
             ) {
                SKSE::log::debug("MarkerClick: vanilla handoff (marker category is disabled).");
                originalSelection(args);
                return;
            }

            // Vanilla resolves quest selections to a regular marker with the same handle.
            // Preserve that behavior when the matching location is already travelable.
            if (marker.type == questMarkerType) {
                for (const auto& candidate : data->mapMarkers) {
                    if (candidate.type == regularMarkerType && candidate.ref == marker.ref && IsDiscoveredMarker(candidate)) {
                        SKSE::log::debug("MarkerClick: vanilla handoff (quest target matches a discovered location).");
                        originalSelection(args);
                        return;
                    }
                }
            }

            // Preserve vanilla cruising before changing the selected destination or opening our dialog.
            // Mount state alone is insufficient: cruise must also be enabled.
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && Native::HasCruiseMountState(player) && Native::IsCruiseEnabled()) {
                SKSE::log::debug("MarkerClick: vanilla handoff (native mount cruising).");
                originalSelection(args);
                return;
            }

            // Resolve the existing marker's engine handle to its real reference.
            // The returned NiPointer keeps that reference alive during this call.
            // Do not create a fake marker or change the reference's position.
            const auto destination = RE::TESObjectREFR::LookupByHandle(marker.ref);

            // Reuse the game's quest display classification before changing the selected destination.
            // A disabled category defers to vanilla; neither the marker nor its reference is modified.
            if (marker.type == questMarkerType) {
                const auto displayType = Native::GetDisplayType(marker);
                const bool doorRouted = displayType == doorQuestDisplayType;
                const bool enabled = doorRouted ? Config::GetAllowDoorQuestMarkers() : Config::GetAllowDirectQuestMarkers();
                SKSE::log::debug(
                    "Quest travel category: displayType=0x{:02X} doorRouted={} enabled={} handle={:08X} ref={:08X}.",
                    displayType, doorRouted, enabled, static_cast<std::uint32_t>(marker.ref),
                    destination ? destination->GetFormID() : 0
                );
                if (!enabled || !destination) {
                    SKSE::log::debug("MarkerClick: vanilla handoff (quest category is disabled or its reference is unresolved).");
                    originalSelection(args);
                    return;
                }
            }

            // TODO: use the linked door's exterior arrival point through native fast travel.
            // Door travel currently uses the existing reference origin and can leave the player in collision.

            // Vanilla clears MapMenu.mapMarker before processing a selection.
            // This is the handle the native confirmation/travel code later reads;
            // it is not the player's persistent custom-marker field.
            auto& selectedDestination = menu->GetRuntimeData()->mapMarker;
            selectedDestination.reset();

            // Ordinary fast travel needs a player and destination, but not the player's current worldspace.
            // Interiors may have no worldspace. Native eligibility decides whether travel is allowed,
            // including changes made by restriction-removal mods such as Unlimited Fast Travel.
            if (!player || !destination) {
                SKSE::log::warn(
                    "MarkerClick: stopped before confirmation (playerPresent={} destinationResolved={} type={} handle={:08X}).",
                    player != nullptr, static_cast<bool>(destination),
                    marker.type, static_cast<std::uint32_t>(marker.ref)
                );
                return;
            }

            // Vanilla checks custom destination borders only in its dragon-cruising selection path.
            // That path is handled by originalSelection above, retaining its native border check.
            // Apply the ordinary native restrictions here without adding a separate border requirement.
            if (!Native::CanFastTravel(player, false)) {
                SKSE::log::debug("MarkerClick: stopped (native player restrictions rejected travel): type={}.", marker.type);
                return;
            }

            if (marker.type == questMarkerType && spdlog::default_logger()->should_log(spdlog::level::debug)) {
                LogQuestDestination(marker, destination.get());
            }

            // Allow the selected map reference without the ordinary discovery requirement.
            // All travel execution and mount decisions remain native.
            selectedDestination = destination->GetHandle();

            // If no dialog is queued, discard the handle.
            // Input has not been suspended, so there is no callback-dependent recovery to perform.
            if (!TravelDialog::Show(menu, marker)) {
                SKSE::log::debug("MarkerClick: stopped (travel confirmation was not queued).");
                selectedDestination.reset();
                return;
            }
            SKSE::log::debug(
                "MarkerClick: custom handling complete; opened expanded travel confirmation: type={} handle={:08X} form={:08X}.",
                marker.type, static_cast<std::uint32_t>(marker.ref), destination->GetFormID()
            );
        }
    }

    bool CreateHook()
    {
        // This feature owns the engine target and function signature.
        // Creation prepares the original trampoline but does not yet activate the detour.
        // plugin.cpp enables the prepared hooks, or resets them if setup fails.
        return Hooks::Create("expanded travel MarkerClick", REL::RelocationID(52208, 53095), &Selection, originalSelection);
    }
}
