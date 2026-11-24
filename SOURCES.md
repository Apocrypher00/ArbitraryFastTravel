# Release sources

Each release tag records the source and dependency versions used to build it. The table below lists the exact revisions.

| Release tag | CommonLibSSE-NG | vcpkg builtin baseline |
| --- | --- | --- |
| [v1.1.0](https://github.com/Apocrypher00/ArbitraryFastTravel/tree/v1.1.0) | 15.6.0, `cce63b4c8aacd2766c21f44b408df92a3ad9e95c` | `00c5775211f45cd08b37fce0484b4cb940e422ab` |
| [v1.0.1](https://github.com/Apocrypher00/ArbitraryFastTravel/tree/v1.0.1) | 7.0.0, `8b032fa992750d654d6d38a33731714d8b86be1f` | `ee12231b20c95013c6638d845d04c91559a1d1ff` |
| [v1.0.0](https://github.com/Apocrypher00/ArbitraryFastTravel/tree/v1.0.0) | 7.0.0, `8b032fa992750d654d6d38a33731714d8b86be1f` | `ee12231b20c95013c6638d845d04c91559a1d1ff` |

Clone with `--recurse-submodules`. If you already have a checkout, run
`git submodule update --init --recursive` after pulling updates. This checks out
the recorded dependency versions; it does not upgrade them to the latest versions.

GitHub's source ZIP does not include submodule contents. Dependency setup may
require internet access. Builds use `x64-windows-static-md`.

Original plugin code is under the Unlicense. CommonLib uses
GPL-3.0-or-later with its Modding and Linking Exceptions; other dependencies
retain their respective licenses. Release ZIPs include their applicable notices.
Historical tags retain the dependency revisions and terms recorded for that tag.

## Finding the source

Project source: [Apocrypher00/ArbitraryFastTravel](https://github.com/Apocrypher00/ArbitraryFastTravel).
For a published release, use its matching release tag rather than the current master branch.
For version 1.1.0, use the `v1.1.0` tag listed above.

Current development dependencies:

- CommonLibSSE-NG 15.6.0: [cce63b4c8aacd2766c21f44b408df92a3ad9e95c](https://github.com/alandtse/CommonLibSSE-NG/tree/cce63b4c8aacd2766c21f44b408df92a3ad9e95c).
- vcpkg baseline: `00c5775211f45cd08b37fce0484b4cb940e422ab`.

These dependencies match CommonLib's own dependency configuration at that revision.

Published releases retain the revisions listed in the table above.
The pinned vcpkg baseline and manifest identify the dependency port recipes, versions,
and upstream source revisions used by the build.

When publishing a binary, provide access to its corresponding source, including the
required linked dependency source and build files, and identify it alongside the download.
Keep any local dependency changes with that release's source; an upstream pin alone
cannot describe an altered dependency. Dependency license texts and this source guide
are included in the binary ZIP under `SKSE/Plugins/ArbitraryFastTravel/Licenses/`.
