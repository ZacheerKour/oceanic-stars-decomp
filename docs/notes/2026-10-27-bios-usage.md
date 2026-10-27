## 2026-10-27 BIOS usage
Script: `SO2_DATA=<checkout with extracted/, analysis/out/d1/, roms/, tools/ref/psxrecomp> python -I analysis/bios_usage.py` (`--selftest` checks the decoders).
It reads the retail dump only for layout (addresses; no bytes printed or committed). Retail dump sha256 = the pin in psxrecomp `bios/SCPH5501.toml` (`11052b64...`).

**Kernel calls used** (stub `addiu t2,0xA0/B0/C0` + `jr/jalr t2` with the `t1` immediate; one site each unless noted):
- boot exe: A0 0x3A `_exit`, 0x44 FlushCache, 0x49 GPU_cw, 0x70 `_bu_init`, 0x72 `_96_remove`; B0 0x07-0x0D events, 0x12-0x15 InitPad/StartPad/StopPad/PAD_init,
  0x17-0x19 ReturnFromException/ResetEntryInt/HookEntryInt, 0x35 write, 0x4A-0x4C InitCard/StartCard/StopCard, 0x56 GetC0Table (2), 0x57 GetB0Table (3), 0x5B ChangeClearPad;
  C0 0x02/0x03 SysEnq/DeqIntRP, 0x0A ChangeClearRCnt.
- A and C (same libcard): A0 0xAB `_card_info`, 0xAC `_card_async_load_directory`; B0 0x32-0x34, 0x36 open/lseek/read/close (stubs 0x8004B064..94), 0x41 format,
  0x42 firstfile, 0x43 nextfile, 0x4E write_card_sector, 0x50 allow_new_card, 0x56 GetC0Table. B: only B0 0x56. The other 356 overlays add nothing new (one A0 site without a readable `t1`).
- Card/pad: the game uses the BIOS card API (B0 0x4A-0x4C, 0x4E, 0x50, A0 0xAB/0xAC, `bu` device via B0 0x32..0x43) and BIOS pad (B0 0x12-0x15, 0x5B). No B0 0x4F read_card_sector:
  card reads go through FileRead on `buXX:`.
- **The 8 `bios_dispatch` rows are Psy-Q libapi kernel patchers**, not ordinary calls: each gets a table, loads one entry and rewrites kernel code at fixed offsets.
  Resolved on SCPH-5501 (B0[0x5B]=0x43D0, C0[6]=0x0C80):
  | site | patch | retail target | psxrecomp SCPH5501 install slot |
  |---|---|---|---|
  | exe 80023E14 | C0[6]+0x70: 3 NOPs over the BIOS card stub | 0x0CF0 | 0xCF0 len 0x10, ok |
  | exe 80023CDC | reads the stub's lui/addiu (target 0x641C, ROM 0x15F0C) and writes 5 words (`jr 0xA000DFAC`) at target+0x28 | 0x6444 | len 0x10; 5th word is a NOP over a ROM NOP, ok |
  | exe 80023D70 | B0[0x5B]+0x9C8: 5 words `jalr 0xA000DF80` | 0x4D98 | len 0x14, ok |
  | exe 800250BC | B0[0x5B]+0x594: 11 words zeroed; saves +0x884/+0x894 (0x4C54/0x4C64) and the exe stub 0x80025094 jumps to +0x894 | 0x4964 | len 0x2C, ok |
  | exe 80025134 | B0[0x5B]+0x62C: 9 words zeroed (7 are ROM instructions) | 0x49FC | **missing**: added by `recomp/bios/SCPH5501_install_slots.patch` |
  | A 8004AEC8, B 8006AF64, C 80070C28 | C0[6]+0x28: compare 6 words, rewrite on match | 0x0CA8 | no write on SCPH-5501: word 5 differs (ROM has `jal 0xEA0` at 0xCB8) |
  Exe 0x80023DCC also copies 0x70 bytes of its own code to kernel RAM 0xDF80 (target of both jump payloads). That RAM is outside the kernel bless window, so the runtime must
  interpret it there (dirty RAM, KSEG1 address); unverified.
- Without the missing slot the patched body fails the kernel-bless check and interprets for the life of the process (slow, still correct; `docs/dynamic_handler_install.md`).

**Device table (DCB+0x34)**: A 0x8004B130..0x8004B344 and C 0x800713B0..0x800715C4 walk `[0x150]` for `[0x154]/0x50` records (`multu 0xCCCCCCCD`, `srl 6`), strcmp the
name, save DCB+0x34 (firstfile), install 0x8004B270 / restore and call the saved one. Retail: kernel ROM 0x12308/0x1231C store table base and size = count*0x50 at 0x150/0x154,
0x50-byte records, +0x34 = firstfile: matches the game. OpenBIOS: same 0x50 record layout (bu DCB: 4 header words + 16 functions, firstFile at +0x34; table RAM 0x5F18..0x6238),
but its boot code (ROM 0x4080) writes **0x6238, the table end, at 0x154**. The game then computes 314 records and walks past the 10 real ones whenever the name is not found.

**OpenBIOS coverage** of the 41 numbers used: all present except **A0 0x3A `_exit`** (table entry is the `unimplemented` trampoline at RAM 0x2AFC; the exe calls it only on exit).
OpenBIOS answers GetB0/C0Table through `patch_hook` (hash-matched replacements: patch_pad, remove_ChgclrPAD, patch_card, patch_card2, send_pad, ...).
Whether SO2's SDK variants hash-match was not checked; without a match the game writes at OpenBIOS B0[0x5B]=0x40A0 + 0x594/0x62C/0x9C8 (0x4634/0x46CC/0x4A68), inside
OpenBIOS kernel code, and jumps to 0x4934.

**Decision**: boot on the user's retail SCPH-5501 (recompiled LLE backend built from the dump on the developer machine; OpenBIOS stays linked but is not allowed for this title).
How psxrecomp consumes it (clone f42d2c0, `docs/BIOS_SELECTION.md`, `docs/config_schema.md` 'BIOS profiles', `runtime/runtime.cmake`):
- `bios/SCPH5501.toml` already describes this exact image (sha256 pin, kernel copy ROM 0x10000..0x18000 -> RAM 0x500, shell -> 0x80030000, 5 install slots); the shell has no
  seeds, so build with `-DPSX_SHELLWIN_INTERP=ON`. Backend: `tools/regen_bios.sh --config bios/SCPH5501.toml` with the dump at `bios/SCPH5501.BIN`; link with
  `-DPSXRECOMP_BIOS_STEMS=OpenBIOS;SCPH5501` (the first non-OpenBIOS stem becomes the expected retail image).
- Runtime picks the BIOS by `--bios <path>` (remembered in `bios.cfg`) or first-run discovery of `SCPH5501.BIN`/`scph5501.bin` in `bios/` next to the exe; the gate is size + CRC32
  (0x8D8CB7E4). `game.toml`: `[recompiler] bios_config = "psxrecomp/bios/SCPH5501.toml"` (game codegen address model) and `[runtime] openbios = false` (retail required).
- `bios_hle` stays off (runtime default false, `runtime/src/main.cpp`); `BIOS_SELECTION.md` calls it "on by default", which the code contradicts.
Not done: no build or run; OpenBIOS `patch_hook` hashes; the 0xDF80 code path; an upstream OpenBIOS fix (store the table size, not its end, at 0x154).

