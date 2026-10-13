# Third-Party Notices

ArbitraryFastTravel-authored source is released under the Unlicense. The combined
plugin binary is governed by GPL-3.0-or-later with CommonLibSSE NG's exceptions.
See `LICENSING.md` for that distinction. Neither grant replaces the licenses of
other third-party projects used to build or run ArbitraryFastTravel.

## Runtime requirements (not redistributed)

| Project | Purpose |
| --- | --- |
| [SKSE64](https://skse.silverlock.org/) | Loads and provides runtime services to the plugin. |
| [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) | Resolves version-specific Skyrim executable addresses. |

## Linked and build dependencies

| Project | License | Copyright / attribution |
| --- | --- | --- |
| [CommonLibSSE NG](https://github.com/alandtse/CommonLibSSE-NG) | GPL-3.0-or-later with Modding and Linking Exceptions | Copyright (c) 2018 Ryan-rsm-McKenzie and subsequent contributors. Includes work from CommonLibSSE and CommonLibSSE-NG. |
| [MinHook](https://github.com/TsudaKageyu/minhook) | 2-clause BSD | Copyright (c) 2009-2017 Tsuda Kageyu. Includes Hacker Disassembler Engine portions by Vyacheslav Patkov. |
| [fmt](https://github.com/fmtlib/fmt) | MIT | Copyright (c) 2012-present Victor Zverovich and fmt contributors. |
| [spdlog](https://github.com/gabime/spdlog) | MIT | Copyright (c) 2016 Gabi Melman. |
| [Xbyak](https://github.com/herumi/xbyak) | 3-clause BSD | Copyright (c) 2007 MITSUNARI Shigeo. |
| [rapidcsv](https://github.com/d99kris/rapidcsv) | 3-clause BSD | Copyright (c) 2017 Kristofer Berggren. |
| [SimpleIni](https://github.com/brofield/simpleini) | MIT | Copyright (c) 2006-2024 Brodie Thiesfield. |
| [toml11](https://github.com/ToruNiina/toml11) | MIT | Copyright (c) 2017 Toru Niina. |
| [JSON for Modern C++](https://github.com/nlohmann/json) | MIT | Copyright (c) 2013-2025 Niels Lohmann. |
| [DirectXMath](https://github.com/microsoft/DirectXMath) | MIT | Copyright (c) Microsoft Corporation. |
| [DirectX Tool Kit](https://github.com/microsoft/DirectXTK) | MIT | Copyright (c) Microsoft Corporation. |

The exact dependency revisions are recorded by the CommonLibSSE-NG Git commit in
`CMakeLists.txt`, `vcpkg.json`, and its builtin baseline. See [SOURCES.md](SOURCES.md).
CommonLib is fetched at that commit; this project does not use a CommonLib submodule.

## Included license texts

The complete license files are packaged beneath:

```text
SKSE/Plugins/ArbitraryFastTravel/Licenses/
```

That folder contains the project Unlicense, the CommonLib GPL text and exceptions,
and the complete installed `share/<package>/copyright` files for each vcpkg dependency
listed above. It also contains the licensing explanation, this notice summary,
and release-source information. This summary does not replace the complete license texts.

Runtime requirements such as SKSE and Address Library are not included in the archive.

Build tools such as CMake, Ninja, vcpkg, Git, and Microsoft Visual C++ are not
redistributed as part of ArbitraryFastTravel.
