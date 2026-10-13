# Release sources

Release source and dependency revisions:

| Release tag | CommonLibSSE-NG | vcpkg builtin baseline |
| --- | --- | --- |
| [v1.0.0](https://github.com/Apocrypher00/ArbitraryFastTravel/tree/v1.0.0) | 7.0.0, `8b032fa992750d654d6d38a33731714d8b86be1f` | `ee12231b20c95013c6638d845d04c91559a1d1ff` |

Each release tag records the plugin source, the exact CommonLib commit in
`CMakeLists.txt`, and the dependency manifest in `vcpkg.json`. CMake downloads
the pinned CommonLib source; no Git submodule initialization is needed.
Builds use `x64-windows-static-md`. Each subsequent release should add a table row
identifying its tag and both dependency revisions.

Original plugin code is under the Unlicense. CommonLib 7.0.0 uses
GPL-3.0-or-later with its Modding and Linking Exceptions; other dependencies
retain their respective licenses. Release ZIPs include their applicable notices.
Historical tags retain the dependency revisions and terms recorded for that tag.

## Finding the source

Project source: [Apocrypher00/ArbitraryFastTravel](https://github.com/Apocrypher00/ArbitraryFastTravel).
For a published release, use its matching release tag rather than the current master branch.
For version 1.0.0, use the `v1.0.0` tag listed above.

Pinned CommonLib source:
[8b032fa992750d654d6d38a33731714d8b86be1f](https://github.com/alandtse/CommonLibSSE-NG/tree/8b032fa992750d654d6d38a33731714d8b86be1f).
The pinned vcpkg baseline and manifest identify the dependency port recipes, versions,
and upstream source revisions used by the build.

When publishing a binary, provide access to its corresponding source, including the
required linked dependency source and build files, and identify it alongside the download.
Keep any local dependency changes with that release's source; an upstream pin alone
cannot describe an altered dependency. Dependency license texts and this source guide
are included in the binary ZIP under `SKSE/Plugins/ArbitraryFastTravel/Licenses/`.
