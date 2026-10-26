# SLZ compression (Star Ocean: The Second Story, PS1)

Normative description of the format as implemented by `analysis/slz.py` (the reference decoder). Verified: all 442 chunks of
all 359 SLZ entries on disc 1 decode to their header size with this grammar (disc 2 holds the same entries). The game's own
decoders are the boot-exe functions at 0x800122b4 (version 1) and 0x8001275c (version 2); they were not diffed against this
spec instruction by instruction, so behaviour on malformed input (section 4) is that of the Python reference, not proven for the console.
All integers are little-endian. Test vectors: `tests/slz/` (synthetic). Encoder: `analysis/slz_encode.py`.

## 1. Entry and chunk layout
An SO2.BIN TOC entry that starts with `SLZ` is a chain of one or more chunks (26 of the 359 entries are multi-chunk). The chunk
index used by the game's loader, and by `config/overlays.tsv`, is the position in the chain, starting at 0.

```
chunk header (16 bytes)
  +0   "SLZ"      magic
  +3   u8  ver    0 = stored, 1 = LZ (terminated), 2 = LZ + fill runs (size terminated)
  +4   u32 csize  payload length in bytes (does not include the 16-byte header)
  +8   u32 dsize  decompressed length in bytes
  +12  u32 next   offset of the next chunk header, relative to THIS chunk's header; 0 = last chunk
  +16  payload
```
* Observed on all 83 non-last chunks: `next == (16 + csize + 3) & ~3` (4-byte aligned); padding bytes are zero. Decoders only need `next`.
* Each chunk has its own `ver`; mixing versions inside one entry is allowed by the format (a test vector does it). Retail: 228 v2, 210 v1, 4 v0 chunks.
* The entry is sector-padded on disc (2048). Bytes after the last chunk are ignored (2 retail entries have non-zero tail bytes).
* `csize` is not read by the reference decoder (the stream terminates itself); it is informational.

## 2. Version 0 (stored)
Output = `payload[0 .. dsize)`.

## 3. Versions 1 and 2: stream grammar
The payload is a sequence of *groups*: one flag byte followed by up to 8 items, one per flag bit, **least-significant bit first**.
A set bit means *literal*, a clear bit means *reference*. The flag byte is fetched lazily, immediately before the first item it covers.

```
group     := flag:u8 item{1..8}
item      := literal | reference
literal   := byte                          ; flag bit = 1, appended to output
reference := b0:u8 b1:u8                   ; flag bit = 0
  f   = b0 | (b1 & 0x0F) << 8              ; 12-bit field
  len = (b1 >> 4) + 3                      ; 3 .. 18
```
**Match** (the normal reference): copy `len` bytes from output position `out.len - f` to the end, **byte by byte**, so
`f < len` overlaps and repeats the pattern (`f = 1` is a run). Requires `1 <= f <= out.len`. Maximum distance 4095.

### Version 1
* `len` is 3..18; every reference is a match.
* **Terminator**: a reference with `f == 0` ends the stream (its `len` nibble is ignored; encoders emit `00 00`). The terminator
  is mandatory; `dsize` is not used to stop. A trailing partially used flag byte is simply abandoned.

### Version 2
* Stops as soon as `out.len >= dsize`, checked **before** reading the next flag or item. A stream whose output is complete
  after the last item of a group has no extra flag byte; an empty chunk has an empty payload. There is no terminator.
* `len == 18` (`b1 >> 4 == 15`) is not a match but a **fill run**:
  * `f < 0x100` (`b1 & 0x0F == 0`): 3-byte form `b0 b1 fill`: `len = b0 + 0x13` (19..274), the fill byte is the third byte.
  * `f >= 0x100`: 2-byte form: `fill = b0`, `len = (b1 & 0x0F) + 3` (4..18).
  * Output gets `len` copies of `fill`.
* Hence ordinary v2 matches have `len` 3..17.

### Pseudocode (both versions)
```
out = []; bits = 0
loop:
  if ver == 2 and len(out) >= dsize: break
  if bits == 0: flag = next_byte(); bits = 8
  lit = flag & 1; flag >>= 1; bits -= 1
  if lit: out.append(next_byte()); continue
  b0, b1 = next_byte(), next_byte()
  f = b0 | (b1 & 15) << 8;  len = (b1 >> 4) + 3
  if ver == 1 and f == 0: break
  if ver == 2 and len == 18:
      if f < 0x100: fill = next_byte(); len = f + 0x13        # third byte follows b0 b1
      else:         fill = f & 255;     len = (f >> 8) + 3
      out += [fill] * len; continue
  repeat len times: out.append(out[len(out) - f])
```
(In `slz.py` the fill-byte read of the 3-byte form is written as `p += 1` before the common `p += 2`.)

## 4. Edge cases and conformance rules
* **Empty chunk**: v0 and v2 have an empty payload; v1 has the terminator group `00 00 00` (flag 0, ref `00 00`), which is what the encoder emits.
* **Overlap** is required semantics, not an optimisation (`ab` then a match with f=2, len=18 gives `ababab...`).
* **Invalid input.** Distance greater than the bytes produced so far makes the Python reference raise `IndexError`; `f == 0` in v2
  silently reads `out[0]` (undefined for the console). A strict decoder must reject both.
* **Overrun.** The reference neither truncates nor checks a match or fill that runs past `dsize`; conforming streams produce exactly
  `dsize` bytes (all retail chunks do). Treat overrun as an error.
* **Truncated payload**: the reference raises `IndexError`; v0 silently returns a short slice. A strict decoder reports an error.
* The window is the whole output (no ring buffer); distance is limited only by the 12-bit field.
* Chunk `dsize` seen: 0 up to 381148 (SO2.BIN entry 2129).

## 5. C reference decoder
Written from this spec. **Not compiled or run** (no C compiler on the authoring machine); verify it against `tests/slz/vectors`
before relying on it. Strict: returns negative on malformed input; `dst_cap` must be at least `dsize`.

```c
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* Decode one chunk at src (src_len bytes available). Returns 0 on success: *out_len = dsize,
   *next = chunk-relative offset of the next chunk (0 = last). */
int slz_decode_chunk(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap, size_t *out_len, uint32_t *next)
{
    if (src_len < 16 || memcmp(src, "SLZ", 3)) return -1;
    unsigned ver = src[3];
    uint32_t n = rd32(src + 8);
    *next = rd32(src + 12);
    if (ver > 2 || n > dst_cap) return -2;
    size_t p = 16, o = 0;
    if (ver == 0) { if (src_len - p < n) return -3; memcpy(dst, src + p, n); *out_len = n; return 0; }
    unsigned flags = 0, bits = 0;
    for (;;) {
        if (ver == 2 && o >= n) break;
        if (bits == 0) { if (p >= src_len) return -3; flags = src[p++]; bits = 8; }
        unsigned lit = flags & 1; flags >>= 1; bits--;
        if (lit) { if (p >= src_len || o >= n) return -3; dst[o++] = src[p++]; continue; }
        if (src_len - p < 2) return -3;
        unsigned f = src[p] | (src[p + 1] & 15u) << 8, len = (src[p + 1] >> 4) + 3;
        if (ver == 1 && f == 0) break;
        if (ver == 2 && len == 18) {
            uint8_t fill;
            if (f < 0x100) { if (src_len - p < 3) return -3; fill = src[p + 2]; len = f + 0x13; p += 3; }
            else           { fill = (uint8_t)f; len = (f >> 8) + 3; p += 2; }
            if (len > n - o) return -4;
            memset(dst + o, fill, len); o += len; continue;
        }
        if (f == 0 || f > o || len > n - o) return -4;
        p += 2;
        for (unsigned i = 0; i < len; i++, o++) dst[o] = dst[o - f];
    }
    if (o != n) return -5;
    *out_len = o;
    return 0;
}
```
Walk an entry: `off = 0; do { decode(src + off, ...); off += next; } while (next);`
