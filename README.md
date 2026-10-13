# Arbitrary Fast Travel

Fast travel to your custom destination directly from Skyrim's world map. Place a custom marker, click it, and choose Yes in the standard fast-travel confirmation dialog.

Optional settings also enable travel to undiscovered locations and quest targets. Discovered locations, fast-travel restrictions, and native dragon cruising retain their original behavior. Confirmation uses the game's localized Yes, No, and Place Marker text.

## Requirements and installation

- Skyrim Special Edition, Steam runtimes **1.6.318, 1.6.323, 1.6.342, 1.6.353, 1.6.629, 1.6.640, 1.6.1130, 1.6.1170, 1.7.99, or 1.7.104**.
- 1.5.97 and VR are not currently supported. 1.6.317 is excluded because it cannot load SKSE plugins.
- SKSE64 matching that runtime.
- Address Library for SKSE Plugins with the database matching your runtime.

Install the release ZIP through MO2 or another mod manager and launch Skyrim through SKSE. The archive contains an SKSE plugin and configuration file; there is no ESP to enable. Restart Skyrim after replacing the DLL or changing settings.

## Configuration

Edit `SKSE/Plugins/ArbitraryFastTravel.ini` inside the installed mod, or `Data/SKSE/Plugins/ArbitraryFastTravel.ini` for a manual installation.

| Section | Setting | Default | Behavior |
| --- | --- | --- | --- |
| General | LogLevel | Info | Trace, Debug, Info, Warn, Err, Critical, or Off. Debug includes quest destination diagnostics. |
| Behavior | AllowCustomDestination | true | Fast travel to the existing custom destination. |
| Behavior | AllowDirectQuestMarkers | false | Fast travel to quest targets displayed directly, usually outdoor objectives. |
| Behavior | AllowDoorQuestMarkers | false | Experimental travel to quest targets represented by a door or the game's routing fallback. |
| Behavior | AllowUndiscoveredLocations | false | Fast travel to undiscovered location markers. |

Setting a behavior option to false delegates that category to the original game code. Discovered locations keep their original path regardless of the undiscovered setting. Missing or invalid settings retain defaults.

Development users upgrading from the earlier `AllowQuestMarkers` option should replace it with the two quest settings above.

## Destination placement

Custom destinations use the point you selected, without checking for a clear landing spot. You are very likely to arrive inside a rock, building, or another placed object if one occupies that point. Some objects let you walk out; others can leave you stuck. Use the console command `tcl` to move clear, then run `tcl` again to restore collision.

## Known limitation: door quest targets

Indoor objectives can appear on the map at an exterior transition door. With `AllowDoorQuestMarkers=true`, fast travel can place you inside the door's collision and leave you stuck. This option is disabled by default. You can use the console command `tcl` to move out, then run `tcl` again to restore collision. Correct door arrival placement is planned for a later release.

The two quest options distinguish the game's door-routed and direct marker categories, rather than literally testing whether the objective is indoors or outdoors. Teleport doors can connect exterior areas too.

## Tested behavior

All ten supported runtimes passed the same smoke test with Debug logging and all behavior options enabled: selection of discovered and undiscovered locations, custom destinations, door-routed and direct quest targets, cancellation of the expanded dialogs, and custom fast travel outside Raven Rock on Solstheim. Quest and undiscovered travel were not completed separately on every runtime.

Broader development tests on 1.6.1170 covered custom destinations, undiscovered locations, outdoor quest targets, both quest category toggles, native cancellation and marker placement, input suspension/restoration, horse mounting, and dragon cruise enabled/disabled. Remote maps selected through WorldMapSelector also worked in the tested setup. These tests do not establish support for every worldspace or mod combination.

## Build and package

Requires Visual Studio with C++ tools, CMake supporting preset schema 6 (3.25 or later), Ninja, and vcpkg. Set `VCPKG_ROOT` to your vcpkg installation and open the folder as a CMake project in Visual Studio.

CMake fetches CommonLibSSE-NG at pinned commit `8b032fa992750d654d6d38a33731714d8b86be1f`; no submodule setup is required. Dependencies use `x64-windows-static-md`.

Configure `x64-Release`, then build `x64-Release-Package` to generate `out/build/x64-Release/ArbitraryFastTravel-0.1.0.zip`. The package includes the default INI, with custom travel enabled, optional categories disabled, and Info logging. Test the resulting ZIP through a separate mod-manager mod before publishing.

## License and sources

Original project source is provided under the Unlicense; see LICENSE. Dependencies retain their own licenses. Release ZIPs include their applicable notices. Exact dependency revisions and release-source guidance are recorded in SOURCES.md.
