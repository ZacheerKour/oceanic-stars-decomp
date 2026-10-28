# Plan: native installer and a single game exe

**Superseded in part (2026-10-27):** no binary is tracked any more. `INSTALL.bat` (plain cmd) finds MSVC or asks Y/N for winget Build Tools, compiles `recomp/installer/install.c` into `.install\setup.exe` and runs it; the player-facing file is `PLAYTHISGAME.exe` (was StarOcean2.exe). Discs go in `roms\`. install.ps1 and the tracked INSTALL.exe/.sha256 are gone. Text below that says INSTALL.exe/INSTALL.bat describes the installer program in general.

Date 2026-10-27. Status: phase 1 implemented (below); the rest is still a plan. Implemented so far: PLAYTHISGAME.exe no longer needs PowerShell (recomp/launcher/play.c,
`--prepare`, `--selftest`; recomp/launcher/equiv.ps1 diffs it against play.ps1). play.ps1 is kept only as the PLAY.bat
fallback when PLAYTHISGAME.exe is absent and as the equivalence reference.

## End state
Clone, drag two disc files onto `INSTALL.bat`, end with one `PLAYTHISGAME.exe` at the repo root that starts the game, shows an
icon, uses relative paths, keeps the right working dir, and stays the foreground process until the game exits (Steam tracks it).
No PowerShell window or script for the player.

## install.ps1 steps, MSVC need, native replacement
| # | install.ps1 step | Needs MSVC? | What INSTALL.bat does instead |
|---|---|---|---|
| 0 | Log to install.log, stamps in .install\ | no | Same, with C file I/O. Stamps = SHA-1 of tracked inputs (Tree-Stamp); hashing via bcrypt.dll (built into Windows). |
| 1 | Discs: drag args or file dialog; .7z/.zip/.cue/.bin/.chd; reject .iso and folders; SHA-1 against two known hashes; copy to roms\discN with a generated .cue | no | GetOpenFileName dialog; bcrypt SHA-1; file copy. zip: miniz or the shell; .7z: embed the LZMA SDK (public domain) or keep fetching the pinned 7zr.exe (SHA-256 checked); .chd: keep the "needs chdman" error, or embed libchdr later. |
| 2 | Tools: MSVC Build Tools via winget (8 GB, UAC), git via winget, private Python 3.11.9 embeddable (SHA-256) | MSVC is the tool itself | See "Compiler" below. git is avoidable: download the GitHub zip of the pinned commit and check SHA-256. Python is only for analysis/extract*.py; download with WinHTTP + SHA-256 as now. |
| 3 | extract_boot.py, extract.py (disc to extracted\, analysis\out) | no | Keep the private Python for now (hidden CreateProcess, output to install.log). Later: port to C (ISO9660 + the game archive format). |
| 4 | Clone psxrecomp at pinned commit f42d2c0; download SDL3 3.4.10 and zlib 1.3.1 tarballs (SHA-256) | no | WinHTTP download of the commit zip and both tarballs, SHA-256 verify, extract with tar.exe (ships with Windows 10+) or miniz/libarchive. |
| 5 | build_all.ps1: recompiler, optional SCPH-5501 backend, AOT overlays, cmake/ninja of rtfull with SDL3 + zlib, psxrecomp-game.exe (about 10 min) | yes: cl, link, cmake, ninja, Windows SDK | Run it hidden with CreateProcess, log to install.log, progress from the "== step" lines. build_all.ps1 stays a script run by the exe, or its steps move into a command table in the exe. Do not touch recomp/rtfull stubs. |
| 6 | Build PLAYTHISGAME.exe (recomp\launcher\build.ps1, cl) | yes | Gone: INSTALL.bat ships the launcher prebuilt (our own code, no game data) as a hash-pinned release asset, or the launcher is merged into the game exe (below). |
| 7 | Desktop shortcut (Y/N) | no | IShellLink in C, plus the Steam hint below. |

## (1) Compiler without asking the player to install Visual Studio
Step 5 is the only real MSVC dependency (plus step 6 today). Options:
1. Detect first: vswhere.exe (in %ProgramFiles(x86)%\Microsoft Visual Studio\Installer) with
   `-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`, falling back to the fixed BuildTools path.
   Use it if found. Always do this.
2. Download MSVC ourselves without the VS installer (msvc-wine style): the public VS manifest (aka.ms/vs/17/release/channel)
   lists the VC tools and Windows SDK packages as .vsix/.msi payloads with SHA-256. INSTALL.bat downloads only the needed
   packages (roughly 1.5-2 GB for cl, link, headers, libs, SDK), verifies each against the manifest, unpacks to tools\msvc and
   sets INCLUDE/LIB/PATH itself. Pros: no admin, far smaller than 8 GB, pinned. Cons: the Microsoft license must be shown and
   accepted (ask Y/N); the manifest format is unofficial; redistribution is not allowed, so download only, never bundle.
3. winget Build Tools (today): official and reliable, but 8 GB, a UAC prompt, long, and may need a restart. Keep as fallback.
4. A different compiler: clang-cl or LLVM-mingw from a pinned GitHub release (hash-checked, redistributable, about 300-600 MB,
   no click-through license). Unknown cost: psxrecomp, the rtfull stubs and SDL3 are validated with MSVC only; it needs a trial
   build and a boot check before it can be recommended.
5. Ship a prebuilt game exe: not possible. It embeds recompiled game code (copyright), so it must be built on the player's machine.

Recommendation: 1, then 2 (own hash-verified download, license shown) as the default, 3 as a fallback behind a Y/N question,
4 as a later research bead.

## (2) Where the final exe lands; merge PLAYTHISGAME.exe and the game exe?
Today PLAYTHISGAME.exe (root) prepares the config, then starts recomp\rtfull\_build_final\psxrecomp-game.exe as a child with args and
env, and waits. The game exe is built per player (game-derived); PLAYTHISGAME.exe is generic.
- Recommended (phase 1): keep two programs. The player-facing file is `PLAYTHISGAME.exe` at the repo root, the current PLAYTHISGAME.exe
  renamed, with an icon resource added at build. It holds no game data, so it can be a prebuilt release asset. It stays the
  foreground process and waits on the child, so Steam sees one running game. Minimal change, already working.
- Phase 2 (optional): link the launcher's prepare()/settings code into the rtfull game exe (call it at the top of main before
  SDL init, use the in-memory Prep, no child). One exe, nothing to wait on. Cost: touches recomp/rtfull (out of scope here), and
  a launcher bug can then break the game binary. Only worth it if Steam overlay or Steam Input misbehaves with the child.
INSTALL.bat's last step puts the exe at the root and writes nothing outside the folder (except the optional shortcut).

## (3) Steam: add as a non-Steam game
Player steps: Steam > Games > Add a Non-Steam Game to My Library > Browse > pick `PLAYTHISGAME.exe` > Add. Steam sets Target to
the exe path and Start In to its folder. What the exe must do:
1. Be a real .exe (done) with an embedded icon and version info so Steam shows the right icon.
2. Resolve every path from its own location, never the current directory (done: `--root` defaults to the exe folder and
   play.c calls SetCurrentDirectory(root)). Settings, saves\ and play\ stay inside the folder.
3. Stay the foreground process until the game exits and return its exit code (done: CreateProcess child +
   WaitForSingleObject). No `start`, no detaching. The .bat files are not usable in Steam; the .exe is the only entry.
4. Create no console window (done: GUI subsystem, CREATE_NO_WINDOW); report errors with a message box.
5. Controller via Steam Input: the game uses an SDL gamepad with hot-plug and a digital pad, which should work with the Steam
   Input virtual XInput pad. Unverified: overlay and Input behaviour with a child process; needs a manual test on a machine
   with Steam. Optional extra: write shortcuts.vdf (binary VDF under Steam\userdata\<id>\config, only while Steam is closed,
   after a backup). Riskier than the manual steps, so default to printing the steps and the exe path, and only write the vdf
   on an explicit Y.

## Suggested beads
1. INSTALL.bat skeleton: log, stamps, bcrypt hashes, hidden CreateProcess runner (replaces Invoke-Logged).
2. Disc intake in C (dialog, zip/7z, SHA-1, cue generation).
3. Downloader: WinHTTP + SHA-256 for the pinned psxrecomp commit zip, SDL3, zlib, Python.
4. Toolchain: vswhere detect, then manifest-based MSVC fetch, then winget fallback.
5. Drive build_all hidden with progress; install the final exe plus icon at the root.
6. Steam steps on the final screen; manual Steam test.

## Phase 1 as implemented (2026-10-27)
- `recomp/installer/install.c` is INSTALL.bat (console app, bcrypt SHA-1/SHA-256, WinHTTP downloads, hidden `cmd /c` runner with
  everything logged to install.log, stamps in `.install\exe_*.stamp`). `INSTALL.bat` is **tracked** (the one allowed binary): it
  holds no game code or data. (Superseded: no hash check exists; `INSTALL.bat.sha256` was never created and `repo_guard.py` rejects
  every `.exe`; the installer is now compiled locally and nothing binary is tracked.) Unsigned, so SmartScreen may warn; `INSTALL.bat` stays as the fallback
  (runs the exe, or install.ps1 when the exe is absent). install.ps1 is kept until the exe has been used by a few people.
- Compiler: vswhere (needs the C++ and CMake components), else the fixed BuildTools path, else winget Build Tools behind Y/N.
  Child scripts honour `SO2_VS`. Option 2 (own manifest-based MSVC download) is NOT implemented: see follow-up below.
- Private Python 3.11.9 and MinGit 2.47.1 (both SHA-256 pinned) replace the winget Git; 7zr.exe is fetched only for .7z.
- PLAYTHISGAME.exe is compiled by INSTALL.bat from play.c (+ play.rc with a generated icon, version info, asInvoker manifest), so
  the launcher is not a prebuilt asset. build_all.ps1 is still run (hidden) as a script; porting it into the exe is a follow-up.
- Follow-up: MSVC without the VS installer (msvc-wine style manifest fetch, per-package SHA-256, show the Microsoft license
  URL/text and require an explicit Y/N, no admin, download only), test with a small fake package first; then port build_all.
