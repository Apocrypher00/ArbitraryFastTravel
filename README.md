# Arbitrary Fast Travel

Fast travel to your custom destination directly from Skyrim's world map. Place a custom marker, click it, and choose Yes in the standard fast-travel confirmation dialog.

Optional settings also enable travel to undiscovered locations and quest targets. Discovered locations, fast-travel restrictions, and native dragon cruising retain their original behavior. Confirmation uses the game's localized Yes, No, and Place Marker text.

## Compatibility

Supported Steam Skyrim runtimes and the SKSE builds present in the test setup:

| Skyrim | SKSE |
| --- | --- |
| `1.5.97` | `2.0.20` |
| `1.6.318` | `2.1.2` |
| `1.6.323` | `2.1.3` |
| `1.6.342` | `2.1.4` |
| `1.6.353` | `2.1.5` |
| `1.6.629` | `2.2.0` |
| `1.6.640` | `2.2.3` |
| `1.6.1130` | `2.2.5` |
| `1.6.1170` | `2.2.8` |
| `1.7.99` | `2.3.0` |
| `1.7.104` | `2.3.1` |

Install the SKSE build and Address Library database matching your exact Skyrim runtime.
Skyrim `1.6.317` is excluded because its matching official SKSE `2.1.0` does not load native DLL plugins.
Other runtimes, GOG, VR, and future Skyrim updates are unsupported unless listed above.

WorldMapSelector remote maps worked in the tested setup. SkyUI is not required.

## Requirements and installation

- [SKSE64](https://skse.silverlock.org/), matching your Skyrim runtime.
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444), with the database matching your runtime.
- [Microsoft Visual C++ Redistributable (x64)](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).

CommonLibSSE-NG, MinHook, and the other build dependencies are linked into the plugin; users do not install them separately.

Install the release ZIP through MO2 or another mod manager and launch Skyrim through SKSE.
For manual installation, copy the archive's contents into Skyrim's `Data` directory.
The archive contains an SKSE plugin and configuration file; there is no ESP to enable.
Restart Skyrim after replacing the DLL or changing settings.

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

All eleven supported runtimes passed the same smoke test with Debug logging and all behavior options enabled: selection of discovered and undiscovered locations, custom destinations, door-routed and direct quest targets, cancellation of the expanded dialogs, and custom fast travel outside Raven Rock on Solstheim. Quest and undiscovered travel were not completed separately on every runtime.

Broader development tests on 1.6.1170 covered custom destinations, undiscovered locations, outdoor quest targets, both quest category toggles, native cancellation and marker placement, input suspension/restoration, horse mounting, and dragon cruise enabled/disabled. Remote maps selected through WorldMapSelector also worked in the tested setup. These tests do not establish support for every worldspace or mod combination.

## Reporting problems

Include `ArbitraryFastTravel.log`, your Skyrim and SKSE versions, the installed plugin version,
your INI settings, a mod list, and the steps needed to reproduce the problem.
Set `LogLevel=Debug`, restart Skyrim, and reproduce the issue to capture selection and quest diagnostics.

The log is normally written to:

```text
Documents/My Games/Skyrim Special Edition/SKSE/ArbitraryFastTravel.log
```

## Build and package

Requires Visual Studio with C++ tools, CMake supporting preset schema 6 (3.25 or later), Ninja, Git, and vcpkg. Set `VCPKG_ROOT` to your vcpkg installation and open the folder as a CMake project in Visual Studio.

CMake fetches CommonLibSSE-NG at pinned commit `8b032fa992750d654d6d38a33731714d8b86be1f`; no submodule setup is required. Dependencies use `x64-windows-static-md`.

Configure `x64-Release`, then build `x64-Release-Package` to generate `out/build/x64-Release/ArbitraryFastTravel-1.0.0.zip`. The package includes the default INI, with custom travel enabled, optional categories disabled, and Info logging. Test the resulting ZIP through a separate mod-manager mod before publishing.

## License and sources

Original project source is provided under the [Unlicense](LICENSE).
The combined DLL is governed by GPL-3.0-or-later with CommonLibSSE-NG's exceptions;
see [LICENSING.md](LICENSING.md) for the distinction.
Other dependencies retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Release ZIPs include the complete license texts and these explanatory files beneath
`SKSE/Plugins/ArbitraryFastTravel/Licenses/`.
Exact dependency revisions and release-source guidance are recorded in [SOURCES.md](SOURCES.md).

## Dependencies and acknowledgements

Development builds on [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG),
including the original CommonLibSSE and CommonLibSSE-NG contributors' work,
[MinHook](https://github.com/TsudaKageyu/minhook) for whole-function detours,
and [SimpleIni](https://github.com/brofield/simpleini) for configuration.
Dependency revisions and license information are recorded in [SOURCES.md](SOURCES.md).

## Development acknowledgement

Arbitrary Fast Travel was developed primarily in collaboration with OpenAI's ChatGPT and Codex.
The author directed the design and reverse engineering, reviewed and revised the source,
and performed the in-game testing. The implementation also builds on research and infrastructure
from WorldMapSelector.
