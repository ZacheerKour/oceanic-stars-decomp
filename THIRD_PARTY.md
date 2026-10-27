# Third-party components

Nothing below is redistributed by this repository. "Fetched" means the installer downloads it from the official source
(SHA-256 pinned in `install.ps1` / `recomp/rtfull/build_deps.ps1` unless noted). "Referenced" means it is only named in
docs or code comments. See [NOTICE](NOTICE) for the summary.

| Component | Version / commit | Licence | How obtained | Redistributed here |
|---|---|---|---|---|
| psxrecomp (RetroPortingToolKit) | `f42d2c004b3dd6ef6ccb83101b40ede55f24bf59` (`f42d2c0`) | PolyForm Noncommercial 1.0.0 | Fetched: `git clone` + checkout of the pinned commit into `tools\psxrecomp` | No |
| OpenBIOS (PCSX-Redux) incl. linked uC-sdk code | as shipped inside the pinned psxrecomp (`bios/openbios.bin`) | MIT (Copyright (c) 2019 PCSX-Redux authors) plus permissive uC-sdk licences; notice in psxrecomp `bios/OpenBIOS.LICENSE` | Fetched with psxrecomp | No |
| SDL3 | 3.4.10 | zlib | Fetched: official release tarball, built locally as a DLL | No |
| zlib | 1.3.1 | zlib | Fetched: official release tarball, built locally (static) | No |
| Python (embeddable, amd64) | 3.11.9 | Python Software Foundation licence | Fetched from python.org into `tools\python` | No |
| 7-Zip `7zr.exe` | 26.05 | GNU LGPL (parts under the unRAR restriction) | Fetched only if no installed 7-Zip is found | No |
| Visual Studio C++ Build Tools, CMake, Git | user's installation | Microsoft / BSD-3-Clause (CMake) / GPL-2.0 (Git) | Referenced: the user installs them | No |
| Docker image with PSX GCC 2.7.2, objdiff, maspsx, splat (decomp track, `INSTALL_DEV.bat` only) | see `docker/Dockerfile` | their own licences | Fetched by the maintainer-only dev installer | No |
| Star Ocean: The Second Story (USA) discs | disc 1 SHA-1 `ccd092233b353d2aa7bb80ecdb2b76f456430b41`, disc 2 `68178c55a430eab3e97eb51ec4a164d36f228aa1` | Copyright tri-Ace / Enix (Square Enix) | Referenced: supplied by the user from their own discs | No |
| Sony PS1 BIOS (optional SCPH-5501) | n/a | Copyright Sony | Referenced: optional, supplied by the user | No |

A game built here links psxrecomp, so it is for noncommercial use only and must not be redistributed (see NOTICE).
When a pin changes, update this table, `NOTICE` and the pins in `install.ps1` in the same commit.
