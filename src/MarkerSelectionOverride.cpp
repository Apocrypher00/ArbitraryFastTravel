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
        constexpr std::uint32_t regularMarkerType     = 0;
        constexpr std::uint32_t questMarkerType       = 1;
        constexpr std::uint32_t customDestinationType = 2;

        // Quest-door display category verified in SE 1.5.97 and the supported AE runtimes.
        // CommonLib agrees on this value.
        constexpr std::uint32_t doorQuestDisplayType = 0x3F;

        // The VR hook receives an engine-owned targeting record.
        // CommonLib currently describes the map-array marker, whose fields have different offsets.
        // This local view names the verified targeting fields and borrows their data.
        struct MarkerInteractionRecordVR
        {
            bool initialized;                     // 00
            bool inUse;                           // 01
            std::byte padding02[6];               // 02: presumed alignment padding
            RE::NiPointer<RE::NiNode> markerNode; // 08: node or derived billboard node
            RE::BSTriShape* markerQuad;           // 10
            RE::RefHandle handle;                 // 18
            std::uint32_t displayType;            // 1C
            std::uint32_t type;                   // 20
            std::uint32_t padding24;              // 24: presumed alignment padding before the label
            RE::BSFixedString label;              // 28
            bool eligible;                        // 30
            std::byte padding31[7];               // 31–37: presumed trailing padding to the 0x38-byte record size
        };
        static_assert(sizeof(MarkerInteractionRecordVR) == 0x38);
        static_assert(offsetof(MarkerInteractionRecordVR, markerNode) == 0x08);
        static_assert(offsetof(MarkerInteractionRecordVR, markerQuad) == 0x10);
        static_assert(offsetof(MarkerInteractionRecordVR, handle) == 0x18);
        static_assert(offsetof(MarkerInteractionRecordVR, label) == 0x28);

        // MinHook fills these with a trampoline:
        // a callable route to the original function after its entry has been redirected below.
        namespace Trampoline
        {
            // The native Scaleform callback is registered as "MarkerClick".
            // Its single argument contains the selected marker index.
            // The function pointer below describes that callback signature, not a new travel API.
            using MarkerSelectionFn = void (*)(const RE::FxDelegateArgs&);
            MarkerSelectionFn markerSelection = nullptr;

            // VR passes the targeted record directly to its selection function.
            using MarkerSelectionFnVR = void (*)(const MarkerInteractionRecordVR*);
            MarkerSelectionFnVR markerSelectionVR = nullptr;
        }

        namespace Native
        {
            // Keep bindings beside their owning feature; startup resolves the required ones before hook activation.
            // These helpers call the engine; they do not reproduce its checks.

            // SE52190/AE53082 selects the native display category from a map-marker record.
            // Quest markers return 0x3F for teleport destinations or the current-location routing fallback.
            // Direct quest variants return 0x3E or 0x40. This classifies routing, not the objective's cell.
            using GetDisplayTypeFn = std::uint32_t (*)(const RE::MapMenuMarker*);
            REL::Relocation<GetDisplayTypeFn> getDisplayType{};

            // SE36877/AE37901 is vanilla's mount-state predicate for cruise selection.
            // It checks the current mount and a process flag; the exact flag meaning is unresolved.
            // Reuse the predicate instead of treating every mounted player as cruising.
            using HasCruiseMountStateFn = bool (*)(RE::PlayerCharacter*);
            REL::Relocation<HasCruiseMountStateFn> hasCruiseMountState{};

            // SE39641/AE40728 reads the cruise setting used by vanilla MarkerSelection.
            // Enabled/disabled dragon tests established its role in that combined condition.
            using IsCruiseEnabledFn = bool (*)();
            REL::Relocation<IsCruiseEnabledFn> isCruiseEnabled{};

            // SE39372/AE40444 applies player restrictions such as combat, weight, and disabled travel.
            // Vanilla MarkerSelection passes false and native refusal messages are shown.
            // The mode argument's full semantics are not named; this is not a height check.
            using CanFastTravelFn = bool (*)(RE::PlayerCharacter*, bool);
            REL::Relocation<CanFastTravelFn> canFastTravel{};

            // VR returns a localized refusal format through a writable pointer.
            // Call the original engine entry so restriction mods can intercept it.
            using CanFastTravelFnVR = bool (*)(RE::PlayerCharacter*, const char**);
            REL::Relocation<CanFastTravelFnVR> canFastTravelVR{};
            REL::Relocation<RE::MapMenu**> mapMenuVR{};
            REL::Relocation<RE::NiPoint3*> placementPositionVR{};

            // SE20109/AE20558 checks destination XY against border regions when enabled by the world.
            // It does not establish terrain height and requires a non-null worldspace.
            // Vanilla uses this in its custom dragon-cruising path, which we delegate to Trampoline::markerSelection.
            // Retained for reference; AFT's ordinary fast-travel path does not call this helper.
            [[maybe_unused]] bool IsPositionWithinBorders(RE::TESWorldSpace* world, const RE::NiPoint3* position)
            {
                using Function = bool (*)(RE::TESWorldSpace*, const RE::NiPoint3*);
                static REL::Relocation<Function> function{ REL::RelocationID(20109, 20558) };
                return function(world, position);
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

        // Expand regular undiscovered, quest, and custom destination eligibility.
        // Vanilla mount cruising retains the entire original selection path.
        // Return false when the hook should hand this selection to its original function.
        // True means AFT consumed it, including a native restriction refusal.
        bool ProcessMarker(RE::MapMenu* menu, const RE::MapMenuMarker& marker, const RE::NiPoint3* placementPosition = nullptr, std::uint32_t cachedDisplayType = 0)
        {
            const char* category = "other";
            if (marker.type == regularMarkerType) { category = "location"; }
            else if (marker.type == questMarkerType) { category = "quest"; }
            else if (marker.type == customDestinationType) { category = "custom destination"; }

            SKSE::log::info(
                "MarkerSelection: selected {} (type={} handle={:08X}).",
                category, marker.type, static_cast<std::uint32_t>(marker.ref)
            );

            // Leave player markers and any unknown categories entirely native.
            if (marker.type != regularMarkerType && marker.type != questMarkerType && marker.type != customDestinationType) {
                SKSE::log::info("MarkerSelection: vanilla handoff (marker category is not overridden).");
                return false;
            }

            // Discovered locations retain their complete vanilla path regardless of configuration.
            if (marker.type == regularMarkerType && IsDiscoveredMarker(marker)) {
                SKSE::log::info("MarkerSelection: vanilla handoff (discovered location).");
                return false;
            }

            // Disabled categories use the complete original selection function.
            // Regular markers reaching this point are undiscovered.
            // This happens before destination changes, cruise checks, or our dialog.
            if (
                (marker.type == customDestinationType && !Config::GetAllowCustomDestination()) ||
                (marker.type == questMarkerType && !Config::GetAllowDirectQuestMarkers() && !Config::GetAllowDoorQuestMarkers()) ||
                (marker.type == regularMarkerType && !Config::GetAllowUndiscoveredLocations())
             ) {
                SKSE::log::info("MarkerSelection: vanilla handoff (marker category is disabled).");
                return false;
            }

            // Vanilla resolves quest selections to a regular marker with the same handle.
            // Preserve that behavior when the matching location is already travelable.
            if (marker.type == questMarkerType) {
                const auto& markers = REL::Module::IsVR() ?
                    menu->GetVRRuntimeData2()->mapMarkers : menu->GetRuntimeData2()->mapMarkers;
                for (const auto& candidate : markers) {
                    if (candidate.type == regularMarkerType && candidate.ref == marker.ref && IsDiscoveredMarker(candidate)) {
                        SKSE::log::info("MarkerSelection: vanilla handoff (quest target matches a discovered location).");
                        return false;
                    }
                }
            }

            // Preserve vanilla cruising before changing the selected destination or opening our dialog.
            // Mount state alone is insufficient: cruise must also be enabled.
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && Native::hasCruiseMountState(player) && Native::isCruiseEnabled()) {
                SKSE::log::info("MarkerSelection: vanilla handoff (native mount cruising).");
                return false;
            }

            // Resolve the existing marker's engine handle to its real reference.
            // The returned NiPointer keeps that reference alive during this call.
            // Do not create a fake marker or change the reference's position.
            const auto destination = RE::TESObjectREFR::LookupByHandle(marker.ref);

            // Reuse the game's quest display classification before changing the selected destination.
            // A disabled category defers to vanilla; neither the marker nor its reference is modified.
            if (marker.type == questMarkerType) {
                const auto displayType = REL::Module::IsVR() ? cachedDisplayType : Native::getDisplayType(&marker);
                const bool doorRouted = displayType == doorQuestDisplayType;
                const bool enabled = doorRouted ? Config::GetAllowDoorQuestMarkers() : Config::GetAllowDirectQuestMarkers();
                SKSE::log::debug(
                    "Quest travel category: displayType=0x{:02X} doorRouted={} enabled={} handle={:08X} ref={:08X}.",
                    displayType, doorRouted, enabled, static_cast<std::uint32_t>(marker.ref),
                    destination ? destination->GetFormID() : 0
                );
                if (!enabled || !destination) {
                    SKSE::log::info("MarkerSelection: vanilla handoff (quest category is disabled or its reference is unresolved).");
                    return false;
                }
            }

            // TODO: use the linked door's exterior arrival point through native fast travel.
            // Door travel currently uses the existing reference origin and can leave the player in collision.

            // Vanilla clears MapMenu.mapMarker before processing a selection.
            // This is the handle the native confirmation/travel code later reads;
            // it is not the player's persistent custom-marker field.
            auto& selectedDestination = REL::Module::IsVR() ? menu->GetVRRuntimeData()->mapMarker : menu->GetRuntimeData()->mapMarker;
            selectedDestination.reset();

            // Ordinary fast travel needs a player and destination, but not the player's current worldspace.
            // Interiors may have no worldspace. Native eligibility decides whether travel is allowed,
            // including changes made by restriction-removal mods such as Unlimited Fast Travel.
            if (!player || !destination) {
                SKSE::log::warn(
                    "MarkerSelection: stopped before confirmation (playerPresent={} destinationResolved={} type={} handle={:08X}).",
                    player != nullptr, static_cast<bool>(destination),
                    marker.type, static_cast<std::uint32_t>(marker.ref)
                );
                return true;
            }

            // Vanilla checks custom destination borders only in its dragon-cruising selection path.
            // That path is handled by Trampoline::markerSelection above, retaining its native border check.
            // Apply the ordinary native restrictions here without adding a separate border requirement.
            const char* refusalText = nullptr;
            const bool allowed = REL::Module::IsVR() ? Native::canFastTravelVR(player, &refusalText) : Native::canFastTravel(player, false);
            if (!allowed) {
                if (REL::Module::IsVR() && refusalText) {
                    TravelDialog::ShowRefusalVR(menu, refusalText, marker.customMarker);
                }
                SKSE::log::info("MarkerSelection: travel refused by native restrictions (type={}).", marker.type);
                return true;
            }

            if (marker.type == questMarkerType && spdlog::default_logger()->should_log(spdlog::level::debug)) {
                LogQuestDestination(marker, destination.get());
            }

            // Allow the selected map reference without the ordinary discovery requirement.
            // All travel execution and mount decisions remain native.
            selectedDestination = destination->GetHandle();

            // If no dialog is queued, discard the handle.
            // Input has not been suspended, so there is no callback-dependent recovery to perform.
            if (!TravelDialog::Show(menu, marker, placementPosition)) {
                SKSE::log::info("MarkerSelection: stopped (travel confirmation was not queued).");
                selectedDestination.reset();
                return true;
            }
            SKSE::log::info(
                "MarkerSelection: AFT opened travel confirmation: type={} handle={:08X} form={:08X}.",
                marker.type, static_cast<std::uint32_t>(marker.ref), destination->GetFormID()
            );
            return true;
        }

        void MarkerSelection(const RE::FxDelegateArgs& args)
        {
            // Start a new log group even for clicks delegated entirely to vanilla.
            SKSE::log::info("MarkerSelection: begin (arguments={}).", args.GetArgCount());

            // FxDelegateArgs exposes the originating handler, which is MapMenu for this registered callback.
            // Reject malformed arguments before indexing the marker array; vanilla's callback assumes valid UI data.
            auto* menu = static_cast<RE::MapMenu*>(args.GetHandler());
            if (!menu || args.GetArgCount() != 1 || !args[0].IsNumber()) {
                SKSE::log::warn("Ignored invalid MarkerSelection arguments.");
                return;
            }
            auto* data = menu->GetRuntimeData2();

            // Scaleform represents numbers as doubles.
            // Ensure this is a finite, nonnegative integer within the array before converting to size_t.
            // These checks are defensive glue, not fast-travel eligibility rules.
            const double index = args[0].GetNumber();
            if (!data || !std::isfinite(index) || index < 0 || std::floor(index) != index ||
                index >= static_cast<double>(data->mapMarkers.size())) {
                SKSE::log::warn("Ignored invalid MarkerSelection index.");
                return;
            }
            const auto& marker = data->mapMarkers[static_cast<std::size_t>(index)];
            SKSE::log::info("MarkerSelection: selected index={}.", index);
            if (!ProcessMarker(menu, marker)) {
                Trampoline::markerSelection(args);
            }
        }

        void MarkerSelectionVR(const MarkerInteractionRecordVR* record)
        {
            SKSE::log::info("MarkerSelection: begin (VR selection).");
            auto* menu = *Native::mapMenuVR;
            if (!menu || !record) {
                SKSE::log::warn("Ignored invalid VR marker selection.");
                return;
            }

            // Adapt only the selected data.
            // The engine's interaction record is borrowed.
            // Its cached display type preserves direct/door classification for quest targets.
            RE::MapMenuMarker marker{};
            marker.ref          = record->handle;
            marker.type         = record->type;
            marker.customMarker = record->label.c_str();
            const auto position = *Native::placementPositionVR;
            if (!ProcessMarker(menu, marker, &position, record->displayType)) {
                Trampoline::markerSelectionVR(record);
            }
        }
    }

    void ResolveNativeFunctions()
    {
        Native::getDisplayType      = REL::RelocationID(52190, 53082);
        Native::hasCruiseMountState = REL::RelocationID(36877, 37901);
        Native::isCruiseEnabled     = REL::RelocationID(39641, 40728);
        if (REL::Module::IsVR()) {
            Native::canFastTravelVR     = REL::ID(39372);
            Native::mapMenuVR           = REL::Offset(0x3014518);
            Native::placementPositionVR = REL::Offset(0x30A6FD0);
        } else {
            Native::canFastTravel = REL::RelocationID(39372, 40444);
        }
    }

    bool CreateHook()
    {
        // This feature owns the engine target and function signature.
        // Creation prepares the original trampoline but does not yet activate the detour.
        // plugin.cpp enables the prepared hooks, or resets them if setup fails.
        if (!REL::Module::IsVR()) {
            return Hooks::Create(
                "MarkerSelection",
                REL::RelocationID(52208, 53095),
                MarkerSelection,
                Trampoline::markerSelection
            );
        } else {
            return Hooks::Create(
                "MarkerSelectionVR",
                REL::ID(52208),
                MarkerSelectionVR,
                Trampoline::markerSelectionVR
            );
        }
    }
}
