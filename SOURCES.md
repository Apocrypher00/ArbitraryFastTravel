# Release sources

No published releases yet. The 0.1.0 release candidate uses:

| Component | Revision |
| --- | --- |
| CommonLibSSE-NG 7.0.0 | `8b032fa992750d654d6d38a33731714d8b86be1f` |
| vcpkg builtin baseline | `ee12231b20c95013c6638d845d04c91559a1d1ff` |

Each release tag records the plugin source, the exact CommonLib commit in
`CMakeLists.txt`, and the dependency manifest in `vcpkg.json`. CMake downloads
the pinned CommonLib source; no Git submodule initialization is needed.
Builds use `x64-windows-static-md`. Future published releases should add a
table row identifying their tag and both dependency revisions.

Original plugin code is under the Unlicense. CommonLib 7.0.0 uses
GPL-3.0-or-later with its Modding and Linking Exceptions; other dependencies
retain their respective licenses. Release ZIPs include their applicable notices.
Historical tags retain the dependency revisions and terms recorded for that tag.
