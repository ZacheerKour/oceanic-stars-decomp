## 2026-10-26 Runtime link audit (boot exe)
**Verdict: NOT SAFE to link, under this task's rules (audited sources only, no downloads, no prebuilt binaries).** Nothing found looks malicious (no obfuscation, no registry or `VirtualProtect`/remote-thread calls, no hidden downloads), but the link closure is the whole runtime, which cannot be audited at the depth this needs, has network, process-spawn and DLL-loading code, and requires SDL3, which is not on this machine except as an unaudited prebuilt. **Nothing was linked or run.** Clone `tools/ref/psxrecomp` at f42d2c0, `git status --porcelain --ignored` empty before and after.

**1. Where the 19 runtime symbols live** (the 15 boot symbols are a subset; definitions by grep in `runtime/src`):
- `memory.c` (2,636 lines): `psx_read_word/half`, `psx_cyc_load_byte`, `psx_cyc_lwc2_read`, `dirty_ram_text_native_ok_ranges(_from)`.
- `traps.c` (1,469): `psx_syscall`, `psx_unknown_dispatch`. `interrupts.c` (2,271): `psx_check_interrupts_at`, `..._dispatch_entry`, `psx_restore_state_escape`. `load_accel.c` (267): `psx_vsync_query_hle_try`. `overlay_loader.c` (5,262): `g_psx_cps_mode`. `gte.cpp` (2,871): `gte_execute`, `gte_write_data/ctrl`, `gte_precision_store_word`.
- `debug_server.c` (**17,699**): `g_debug_last_store_pc`, `debug_server_cyc_observe`, `debug_server_log_call_entry`.

**2. Dependency closure.** These 7 files include about 100 runtime headers: GPU/GL renderer, SPU, CD-ROM, MDEC, savestates, mods, netplay, OpenXR, rewind, the BIOS image, the overlay loader and the debug server. Their callees span the runtime. `runtime/src` + `include` hold 440 files and about 170,000 lines in `src` alone. So "only the files that define the symbols" is not a link set; the link set is the runtime library (`runtime/runtime.cmake`, 154 KB of CMake), which needs:
- **SDL3 3.4+** (`runtime.cmake:252-316`: `find_package`, then a FetchContent of `SDL3-3.4.10.tar.gz` from GitHub, else `FATAL_ERROR`). `debug_server.c:119` includes `psx_sdl.h` and uses `SDL_Thread`/`SDL_mutex`, so even a hand-made link list needs SDL3 headers and library. SDL3 exists here only as a **prebuilt MinGW library** inside `tools/recomp/dl/cmake-clang-v1-windows-x64.zip` (unaudited, wrong ABI for MSVC). **Blocked: it needs a download (hash-pinned in `third_party/deps.manifest`, sha256 `12b34280...`) or a prebuilt.**
- zlib (`runtime.cmake:976-1007`, same find-or-fetch pattern), Threads, OpenGL; OpenXR, libwebp, libjuice and recomp-net only behind options.
- Vendored in the runtime: `third_party/stb_image.h`, `xxhash.h` (not read).

**3. Pattern sweep over `runtime/src` + `runtime/include` (440 files)**:
- **Network** (58 hits, 9 files): `main.cpp` contacts **`api.ipify.org:80`** (`ae_np_external_ip`, :15294, the public-IP lookup in the netplay launcher's callback table, :16605) and binds UDP sockets for LAN discovery and netplay (`--net-bind`, default `0.0.0.0:7777`). `psx_lobby_client.c` + `lobby_ws/` hold a WebSocket client; the default lobby is `ws://netplay.retcomm.net:8765`. `debug_server.c` has a TCP debug server bound to `INADDR_LOOPBACK` (:17012). Also `beetle_debug_server.c` and `cosim.c`.
- **Process spawn** (7 files): `autocompile.c:1204` (`cmd -> python -> gcc` at run time, below-normal priority, in a job object), `main.cpp:11419` (`system("python tools/prepare_disc.py ...")`), `psx_bios_module.c:380/418` (`CreateProcessA`/`popen` of a BIOS build command), `host_overlay.c:314` (relaunches its own exe), `mod_runtime.cpp:876`, `debug_server.c:15407` (`system("mkdir ...")`).
- **Dynamic loading:** `overlay_loader.c` `LoadLibraryA` of compiled overlay DLLs (5 sites), `psx_bios_module.c:247`, `host_sha256.h:29` (`bcrypt.dll` from System32 only).
- **File writes/deletes** (61 delete/rename sites in 14 files): savestates, memcards, mod packages, overlay capture, `atomic_file_write.h`. Not traced to a single directory.
- 307 `getenv` sites in 45 files (91 in `main.cpp`).
- None: registry, `VirtualProtect`/`VirtualAlloc`/`WriteProcessMemory`/`CreateRemoteThread`, lines over 400 characters, base64-like runs of 200 or more characters.

**Read in full:** none of the 170,000 lines; only the call-site contexts quoted above. This is a capability map, not an audit.

**What would unblock a link** (decisions for you):
1. Supply SDL3 3.4.10 and zlib 1.3.1 source archives yourself, checked against the `deps.manifest` hashes (no download by an agent), and approve a full audit of the runtime. That is a multi-session job; `main.cpp`, `debug_server.c` and `overlay_loader.c` are the risk centres.
2. Or build the runtime only inside Windows Sandbox (networking off). That still needs SDL3: from the pinned toolchain zip (prebuilt, used there before for clang only), or from a source tarball you supply.
3. Or write our own minimal shim for the 19 symbols plus `main` to test the generated C alone. It would not test psxrecomp's runtime, and a boot needs a BIOS/HLE layer that a shim lacks.

**BIOS:** not reached. psxrecomp bundles `bios/openbios.bin` (unvetted blob, see the safety audit); per the `PSXRECOMP_ALLOW_NO_BIOS`/`PSXRECOMP_BIOS_STALE_FATAL` options the runtime expects a BIOS image. Never download one: if a later run needs one, you supply your own SCPH1001 dump.

