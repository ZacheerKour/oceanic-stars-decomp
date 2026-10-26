# recomp/sandbox - isolated psxrecomp analyzer run (UNTESTED)

Purpose: build **only** `psxrecomp-analyze` and run it on the boot exe `SCUS_944.21`, inside Windows Sandbox, with no network,
and get counts / jump-table findings / unresolved indirect jumps back. Nothing from `tools\recomp` or `tools\ref\psxrecomp` is run or built on the host.
Why: the upstream codegen host downloads its toolchain from `releases/latest` without a hash check and the toolchain's `install.ps1` edits the user PATH
(`tools\ref\SECURITY_TRIAGE.md`, gitignored). Here the toolchain zip is pinned by SHA-256 and never installed.

**Status: nothing in this folder has been run in a sandbox.** Tested only: both scripts parse (PowerShell parser), and `make_wsb.ps1` runs on the host
(hashes the zip and the exe, writes the gitignored files). Everything inside `run_in_sandbox.ps1` is untested, including the compiler choice and the cmake options.

## Files
| File | What |
|---|---|
| `make_wsb.ps1` | host side; checks zip + exe SHA-256, creates `_stage\` and `_out\`, writes `_run.wsb` with this checkout's real paths. Does not start the sandbox. |
| `run_in_sandbox.ps1` | guest side (LogonCommand). Copied into `_stage\` by `make_wsb.ps1`. |
| `_stage\`, `_out\`, `_run.wsb` | generated, gitignored (`/recomp/sandbox/_*`) |

## Steps
1. Review `run_in_sandbox.ps1` (the sandbox runs the staged copy; `make_wsb.ps1` prints its SHA-256).
2. `powershell -NoProfile -ExecutionPolicy Bypass -File recomp\sandbox\make_wsb.ps1` (per-process bypass; no system setting is changed).
3. Review `recomp\sandbox\_run.wsb` and `_stage\`, then open `_run.wsb` yourself. Do not run the other files.
4. Wait for `_out\status.txt` to read `OK` or `FAILED: ...` (the build is the long step; the toolchain extract is 16k files, 761 MB). Close the sandbox; everything inside is discarded.
5. Read `_out\summary.txt`, `run.log`, `analyze_*.log`, `seeded\` and `exact\` (`functions.tsv`, `symbol_addrs.txt`). Treat `_out` as untrusted data: read it, do not execute anything in it.

## Sandbox settings and mappings (see `_run.wsb`)
Networking, vGPU, audio/video input, printer and clipboard redirection disabled; ProtectedClient on; 8 GB RAM.
Read-only: `tools\ref\psxrecomp` -> `C:\in\src`, `tools\recomp\dl` -> `C:\in\dl`, `_stage` -> `C:\in\stage`. Writable: `_out` -> `C:\out`.
Deviation from the minimal spec: `_stage` holds `SCUS_944.21` plus `seeds.txt` (copy of `config\seeds.txt`: addresses only, 545 lines) and `run_in_sandbox.ps1`,
because the analyzer takes seeds as a file and the logon script must come from somewhere. The writable `_out` is the only channel from the sandbox to the host.

## What `run_in_sandbox.ps1` does
1. Copies the exe and zip to `C:\work`, then verifies SHA-256 of the local copies (exe `b93e4246...`, zip `28da9742...`).
2. Extracts the zip with .NET `ZipFile` to `C:\work\tc`, rejecting entries that escape the target. `install.ps1` is never run.
3. Session-only `PSXRECOMP_TOOLCHAIN_DIR=C:\work\tc` and `PATH=C:\work\tc\bin;...`. `psxrecomp_cli.py` is not used (its `analyze` calls `ensure_toolchain`/`ensure_analyzer`, which can fetch things); `cmake` is called directly.
4. Copies the source to `C:\work\src` without `.git`.
5. `cmake -S recompiler -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_ENABLE_CHD=OFF -DBUILD_TESTING=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`, then `--target psxrecomp-analyze`.
6. Runs the analyzer twice: whole-image scan with our seeds, and `--exact` (reachability only), both with `--no-refs --emit-tsv --emit-symbol-addrs`. This mirrors what `psxrecomp_cli.py analyze` passes. The analyzer has no `game.toml` input: the `game.toml` values (entry, load address, size, sp) come from the exe header, and the summary prints them for comparison with `recomp\game.toml`.
7. Writes `summary.txt` (stats, indirect sites by class, jump-table sites, unresolved sites as pc/reg/kind) and copies only `functions.tsv` and `symbol_addrs.txt` out. The analysis JSON (its `context` field holds disassembly text) stays inside the sandbox.

## Network / submodule needs (nothing was fetched)
* Root `.gitmodules`: `lib/recomp-net` (https://github.com/RetroPortingToolKit/recomp-net.git) and `lib/retcomm-rbengine` (https://github.com/RetroPortingToolKit/rbengine.git). Not checked out in the clone; per `docs/BUILDING.md` they are needed only for `-DPSX_NETPLAY=ON` / the launcher UI (`recomp-ui`, which is also absent). **Not needed for the analyzer.** Pinned submodule commits (from `git submodule status`): recomp-net `e53059306824edf4f0aec4aae5ae65151b18535f`, rbengine `2a03e73693acee0fb78076ea058642c931bef12e`.
* `recompiler/lib/{fmt,rabbitizer,toml11,ELFIO}` are already present as plain files in the clone (checked: no `.git` inside), so the analyzer needs no submodule.
* One network dependency remains in the recompiler CMake: `FetchContent` of **libchdr** (`runtime/chd_dependency.cmake`, a URL with a pinned hash), used only by the `psxrecomp` media CLI. `-DPSXRECOMP_ENABLE_CHD=OFF` skips it. If configure still reaches for it, the run fails offline rather than fetching; staging libchdr outside the sandbox needs your OK.
* The full runtime / `generate` path (SDL3 fetch, OpenBIOS, per-title repo) is out of scope here.
