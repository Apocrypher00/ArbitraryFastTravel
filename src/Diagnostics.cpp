#include "PCH.h"
#include "Diagnostics.h"
#include "Config.h"

namespace Diagnostics
{
    void LogQuestDestination(const RE::MapMenuMarker& marker, RE::TESObjectREFR* reference)
    {
        // Skip reference inspection when Debug messages are disabled.
        if (Config::GetLogLevel() > spdlog::level::debug) { return; }

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

}
