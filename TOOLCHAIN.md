# Toolchain notes (2026-10-25)

## Existing SO2 PS1 decomp
None found (web search + GitHub repo search "star ocean second story decomp": 0 results). Closest references for method:
- Legend of Mana (matching, 100%): https://github.com/celophi/lom-decomp  (Docker dev image; GCC 2.8.0/2.7.2-cdk/2.6.0 psx builds from old-gcc, ASPSX 2.77/2.67/2.34, maspsx, splat, objdiff; MIT for original material only)
- Ridge Racer: https://github.com/kazuyette/rr-decomp (GCC 2.5.7 + maspsx, PsyQ libs kept as asm)
- Evo's Space Adventures https://github.com/mkst/esa ; Mega Man X4 https://github.com/sozud/mmx4

## Exe facts
- extracted/SCUS_944.21 is only 128 KB (PS-X EXE). Game logic is surely in overlays/archives on disc (not extracted yet; discs are .7z in project root).
- Strings: only Sony BIOS/kernel `$Id` lines (intr.c 1997/02, sys.c 1998/01, bios.c 1996/12) and "Library Programs (c) 1993-1997". No explicit PsyQ/GCC version string. Era suggests Psy-Q 4.x; compiler (likely GCC 2.7.2 or 2.8.x psx) must be determined by trial matching on a function.

## Installed (all under tools\)
| Item | Version | Source | Size |
|---|---|---|---|
| splat64 | 0.50.1 | pip --user | - |
| objdiff.exe (GUI) | 3.8.3 | github.com/encounter/objdiff/releases/download/v3.8.3/objdiff-windows-x86_64.exe | 24.3 MB |
| objdiff-cli.exe | 3.8.3 | .../objdiff-cli-windows-x86_64.exe | 7.2 MB |
| Ghidra (extracted) | 12.2 PUBLIC 20261016 | github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.2_build/ghidra_12.2_PUBLIC_20261016.zip | 569.7 MB |
| ghidra_psx_ldr | built for 12.1.4 | github.com/lab313ru/ghidra_psx_ldr/releases/download/2026.10.03/ghidra_12.1.4_PUBLIC_20261004_ghidra_psx_ldr.zip | 9.2 MB |
| maspsx (git, source) | HEAD | github.com/mkst/maspsx | - |
| decomp-permuter (git) | HEAD | github.com/simonlindholm/decomp-permuter | - |
| old-gcc (git, build scripts) | HEAD | github.com/decompals/old-gcc | - |

Notes:
- psx_ldr zip copied to ghidra_12.2_PUBLIC\Extensions\Ghidra but it targets 12.1.4; Ghidra 12.2 may reject it. No 12.2 build exists yet; fix = install Ghidra 12.1.4 instead.
- Ghidra cannot run: `java` not found (needs JDK 21). Not installed, per instructions.

## Update 2026-10-25 (Docker + JDK + Ghidra 12.1.4 done)
- Docker image `so2-decomp` (docker\Dockerfile, ubuntu:22.04): binutils-mipsel-linux-gnu 2.38, python3, splat64, make/ninja, old-gcc 0.17 prebuilts in /opt/gcc-{2.6.0-psx,2.7.2-psx,2.7.2-cdk,2.8.0-psx,2.8.1-psx} (cc1 at /opt/gcc-X/cc1). Run `.\dockerun.ps1 <cmd>` (no args = bash); project mounted at /work, maspsx used from /work/tools/maspsx/maspsx.py (not copied into image). Verified: each cc1 compiles a C file -> .s -> .o via maspsx/GNU as.
- ASPSX/wibo NOT installed: maspsx replaces ASPSX with GNU as; original aspsx exes (mkst/maspsx release aspsx) are DOS and only needed to cross-check. Add later if matching needs it.
- JDK: tools\jdk-21 = Temurin 21.0.9+10 (OpenJDK21U-jdk_x64_windows_hotspot_21.0.9_10.zip, adoptium/temurin21-binaries, 205 MB). Not on PATH; set JAVA_HOME=tools\jdk-21 per process.
- Ghidra: tools\ghidra-12.1.4 (ghidra_12.1.4_PUBLIC_20260921.zip, NSA GitHub Ghidra_12.1.4_build, 570 MB). ghidra_psx_ldr unzipped into its Ghidra\Extensions\ghidra_psx_ldr. 12.2 left in tools\ghidra (unused, psx_ldr incompatible).
- Headless verified (JAVA_HOME=tools\jdk-21, analyzeHeadless ... -import extracted\SCUS_944.21 -noanalysis, project dir tools\ghidra-projects): "Using Loader: PSX Executables Loader", language PSX:LE:32:default:default, import succeeded.

## Remaining
- Psy-Q SDK libs: no clean source; not fetched.
- Which GCC matches the game is undetermined (needs trial matching).
