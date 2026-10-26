# Overlay schema (`config/overlays.json`, schema `so2 overlays v1`)

Stable, machine-readable form of `config/overlays.tsv` (human notes stay in ANALYSIS.md section 6). Generated, never hand-edited:

```
python -I analysis/overlay_json.py          # tsv -> json; sha256 from the local discs (kept from the old json if roms\ is absent)
python -I scripts/make_psxrecomp_overlays.py  # json -> recomp/aot/overlays.json (psxrecomp AOT profile)
python -I tests/slz/check_local.py          # re-decode every overlay from the discs and verify md5 + sha256 (skips without roms\)
```
No game bytes are stored: ids, addresses, sizes and hashes only.

## Top level
| Field | Meaning |
|---|---|
| `schema` | `"so2 overlays v1"` |
| `game`, `container`, `serials` | `Star Ocean: The Second Story (USA)`, `SO2.BIN`, `SCUS-94421` / `SCUS-94422` |
| `overlays[]` | one record per decompressed chunk (141) plus the boot exe record (142 total) |

## `overlays[]` record
| Field | Type | Meaning |
|---|---|---|
| `id` | string | `boot_exe`, or `e<toc_entry>c<chunk>` (e.g. `e1984c1`). Unique, stable. |
| `discs` | int[] | discs holding it (all records are on `[1, 2]`: code is byte-identical on both discs) |
| `toc_entry` | int / null | SO2.BIN TOC index (docs/SO2BIN_TOC.md); null for the boot exe (ISO file `SCUS_944.21`) |
| `chunk` | int | SLZ chunk index inside the entry (docs/SLZ.md); 0 for the exe |
| `load_addr` | string | KSEG0 address the decompressed bytes are placed at, `0x...` |
| `size` | int | decompressed bytes |
| `sha256` | string | SHA-256 of the decompressed bytes (for `boot_exe`: the 0x1F800-byte text image after the 0x800 PS-X header) |
| `md5` | string | MD5 of the same bytes (as in the tsv) |
| `role` | string | free-text role |
| `confidence` | `high` / `medium` | confidence of `load_addr` (basis below) |
| `basis` | string | evidence codes from overlay_map.py: `LIT` literal address in loader code, `VOTE` jal-target/function-start alignment, `RULE`, `SLOT` relocation slot, `PS-X header` |
| `identical_to` | string[] | other ids with the same md5 (symmetric; may sit at another address: slot-relocated copies of 2131-family have different md5 per slot) |
| `loader` | string / null | how the game selects/loads it (call site, free text) |

Overlays share addresses (e.g. nine alternative `0x8008c1f4` battle handlers, the `0x8007e000` area modules, the four 2131-family slots):
group by `load_addr` to find the alternatives that can never be resident together. Coexistence is **not** otherwise encoded (open item).

## psxrecomp mapping (`scripts/make_psxrecomp_overlays.py`)
Checked against `tools/aot_overlay_pipeline.py` (psxrecomp HEAD f42d2c0), which requires `schema == "psxrecomp AOT methods v1"`, `game_id` equal to `[game] id`,
exactly one of `disc_hashes` / `disc_set`, `images[]`, `expected_records`, and optional `checks[]`.

| Our field | psxrecomp field | Status |
|---|---|---|
| (constant) | `schema = "psxrecomp AOT methods v1"` | real |
| serials[0] | `game_id` | real field; value must equal game.toml `[game] id` |
| two discs | `disc_set[{name, disc_hashes{sha256}}]` | real shape; **hash values are placeholders** (`TODO(verify)`) |
| boot exe | `images[{method:"psx_exe", files:["SCUS_944.21"]}]` | real method |
| overlays | `images[{method:"slz_members", container, members[]}]` | **GUESSED**: no SLZ/SO2.BIN method exists upstream; we propose it (see RECOMP_NOTES.md gaps) |
| `id` | `members[].name` | guessed |
| `toc_entry`, `chunk` | `members[].toc_entry`, `.chunk` | guessed |
| `load_addr` | `members[].load_addr` | name follows the real `load_addr` key of other methods |
| `sha256` | `members[].decoded_sha256` | name follows upstream `decoded_sha256` (aligned_lzss_banks placements) |
| `identical_to` at same `load_addr` | `members[].aliases` | name follows upstream `aliases`; we merge identical (addr, bytes) like psxrecomp's own dedupe |
| `confidence` | `x_confidence` | ours (ignored by upstream; `x_` prefix) |
| count | `expected_records` | upstream requires the exact number of recipes; our count (1 exe + 89 unique images) is a guess at how it will count |

Not yet encoded: per-overlay `dispatch_entries` / entry points (needs run-time capture, RECOMP_NOTES.md checklist 6), `checks[]` (loader-word verification),
`compositions` (the 1984 chunks 0+1 and 1981 are physically adjacent in RAM).
