# Arbitrary Fast Travel

SKSE plugin in development to allow native fast travel to the existing custom destination marker. The current source is an experimental custom-marker travel prototype using the native confirmation and travel path. It has not yet been tested in game.

## Build and package

Requires Visual Studio with C++ tools, CMake, Ninja, and vcpkg. Set VCPKG_ROOT to your vcpkg installation and open the folder as a CMake project in Visual Studio.

CommonLibSSE-NG is fetched by CMake at pinned commit 8b032fa992750d654d6d38a33731714d8b86be1f. No submodule setup is required.

Configure x64-Release, then build x64-Release-Package to generate an installable ZIP. Install through a separate MO2 test mod and restart Skyrim after DLL changes.

The prototype currently advertises Steam Skyrim 1.6.1170 only. Full clean-build and in-game validation remain pending. End-user runtime requirements are matching SKSE and Address Library.

## Intended behavior

Use the existing custom destination reference, native confirmation dialog, and engine travel path. Preserve regular-marker behavior, player fast-travel restrictions, and dragon cruise. Quest-marker and undiscovered-location options are future work.

## License

Original project source is provided under the Unlicense; see LICENSE. Dependencies retain their own licenses. The packaging script includes notices for the linked dependencies; the project's Unlicense does not replace those terms.

Exact dependency revisions and release-source guidance are recorded in SOURCES.md.
