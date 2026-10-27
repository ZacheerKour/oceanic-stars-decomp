## 2026-10-26 SLZ overlays into the AOT pipeline
**Decision: neither the captures file nor an `slz_members` image method, but a `prepared_files` decoder.** psxrecomp's `tools/prepared_files.py` already lets a
profile derive virtual files from disc bytes through a registered decoder (`METHODS`, today only `bpe_container`). Each output is pinned by `consumed`,
`source_sha256`, `size` and `sha256`, and is then an ordinary file for `fixed_address_files`. One SLZ chunk is exactly such a transform.

| | captures file (`compile_overlays.py --captures`) | `slz_members` method (fork of `aot_overlay_pipeline.py`) | **`prepared_files` method `so2_slz` (chosen)** |
|---|---|---|---|
| upstream change | none | new `spec_sources` branch + TOC decryption + pinning, in a 1234-line file | 2 lines in `prepared_files.py`: `import slz` and `'so2_slz': (slz.prepared, ())` |
| gates kept | none of the pipeline's (no disc hash, image hash, inventory, audit or staging); records are hand-built | all, if written right | all: disc hash, per-chunk input and output hashes, inventory, audit, staging |
| game bytes | a JSON holding the decompressed code (`bytes_b64`) to keep privately | none stored | none stored: rebuilt from the user's disc on each run; the profile holds offsets, sizes, hashes |
| TOC | needed | needed (port `toc.py`) | not needed: each chunk's byte offset in SO2.BIN is pinned and its bytes are hash-checked |

Implementation:
- `analysis/slz.py` `prepared(data, spec)`: the decoder contract (`data` starts at the chunk header; returns `(output, consumed = 16 + csize)`). Strict: bad magic, a broken stream, output size != header `dsize`, or a stream that reads past the declared payload raise `ValueError` (the one exception `prepare()` turns into `PreparedError`). `dec`/`dec_all` behave as before.
- `analysis/aot_profile.py` writes `recomp/aot/overlays.json`: 89 unique (load_addr, bytes) chunks as `prepared_files` (`SLZ/E<entry>C<chunk>`, `source {file: SO2.BIN, file_offset}`), 52 identical copies listed in `x_aliases`, 14 `fixed_address_files` images (one per load address, `allow_missing: true` because the generic extractor cannot see compressed code), plus `psx_exe` for the boot exe. It decodes every entry again from the local disc and checks it the way `prepare()` does, and against `config/overlays.json`; it also self-tests `prepared` on the 18 synthetic vectors and 4 malformed inputs. Output: `89 prepared SLZ chunks at 14 load addresses, 52 aliases; all re-decoded and hash-checked`. `python -I tests/slz/run_tests.py`: `OK 0`.
- **Disc 1 only** (`disc_hashes`, not `disc_set`): disc 2's ISO directory has no SO2.BIN (ANALYSIS.md section 1), so `Disc.read('SO2.BIN')` would fail there. All 141 code chunks are byte-identical on disc 2, and shards are keyed and guarded by bytes, so shards built from disc 1 serve both discs.
- This supersedes the `slz_members` profile of `scripts/make_psxrecomp_overlays.py`. Do not rerun that script: it would overwrite the new profile (editing `scripts/` was outside this task).

Open: nothing of psxrecomp was run (project rule: no psxrecomp code on the host; the sandbox harness covers only the analyzer). To use it in the private title repo: put `analysis/slz.py` next to `psxrecomp/tools/prepared_files.py` (or on its import path), add the two lines, then run `aot_overlay_pipeline.py extract --profile aot/overlays.json --game-toml game.toml --disc "<Disc 1>.cue" ...`. Unverified: `expected_records`; whether the images at medium-confidence load addresses (0x801F9800 slot family, e3107c0) compile cleanly; per-overlay entry points (none declared; the pipeline finds prologues, `jal` roots and frameless leaves itself; `config/seeds_overlays.txt` can become `entries` once a run shows what it misses).

