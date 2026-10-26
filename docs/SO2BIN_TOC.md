# SO2.BIN table of contents (encrypted)

`SO2.BIN` (ISO9660 file starting at LBA 300, 483,493,888 bytes on disc 1) is an archive of up to 4608 entries addressed by index,
not by name. Same format on both discs (disc 2 has no ISO directory entry for SO2.BIN, but the TOC is read from absolute sector 300 on both). Reference implementation: `analysis/toc.py` (`load`, `ent`). The exe reads and decrypts it
in the function at 0x80011c20 (decrypt loop from 0x80011cc4): 0x6000 bytes starting at sector 300, i.e. the first 12 sectors of the file.

## 1. Raw layout (0x6000 bytes = 12 sectors of 2048)
| Offset | Size | Content |
|---|---|---|
| 0x0000 | 0x1200 x u32 | slot words `w[i]` (4608 slots) |
| 0x4800 | 0x1200 x u8 | size-low bytes `sz[i]` |

Sectors are ISO user data, 2048 bytes (in the raw 2352-byte BIN images: offset 24 of each Mode 2 Form 1 sector).

## 2. Decryption
A 32-bit generator, all arithmetic modulo 2^32:
```
K = 0x13578642
a = K
for i in 0 .. 0x11FF:
    w[i]  ^= a
    a      = a ^ (a << 1)
    sz[i] ^= a & 0xFF
    a      = (~a) ^ K
```
Check: after decryption `w[0] == 0x34829314` (magic) and `sz[0] == 0`.

## 3. Slot format
For `i >= 1`, after decryption:
```
byte0 = BCD minute    (w & 0xFF)
byte1 = BCD second    (w >> 8 & 0xFF)
byte2 = BCD frame     (w >> 16 & 0xFF)          ; bcd(x) = (x >> 4) * 10 + (x & 15)
byte3 = size, high byte  (w >> 24)
LBA     = (min * 60 + sec) * 75 + frame - 150   ; absolute disc LBA (MSF minus the 2 s lead-in), NOT relative to SO2.BIN
sectors = byte3 << 8 | sz[i]                    ; 0 = empty slot
bytes   = sectors * 2048                        ; sector granular, no byte-exact size
```
Example (disc 1, entry 1): `w = 0x00120600` -> MSF 00:06:12 -> LBA 312; `sz = 63` -> 63 sectors. Entry 1 is the first entry, directly
after the TOC (312 = 300 + 12).

## 4. Properties
Disc 1 / disc 2 (details in ANALYSIS.md section 2):
* 4155 / 4170 non-empty entries (max index 4586 / 4604). Index 0 is the header slot.
* Entries are contiguous with no gaps or overlaps; LBAs span 312..222881 on disc 1.
* LBAs are absolute, so slots differ between discs even for identical content; 4136 entries have the same index and bytes on both discs.
* An entry starting with `SLZ` is compressed (see SLZ.md); otherwise it is raw (data containers, STR video).

## 5. Pseudocode
```
def read_toc(disc):                       # disc.read(lba, nbytes) returns ISO user data
    raw = disc.read(300, 0x6000)
    w   = u32_array(raw[0:0x4800]);  sz = list(raw[0x4800:0x6000])
    decrypt(w, sz)                        # section 2
    return w, sz
def entry(w, sz, i):
    m, s, f = bcd(w[i] & 255), bcd(w[i] >> 8 & 255), bcd(w[i] >> 16 & 255)
    return (m*60 + s)*75 + f - 150, (w[i] >> 24) << 8 | sz[i]     # (lba, sectors)
```
```c
void so2_toc_decrypt(uint32_t w[0x1200], uint8_t sz[0x1200]) {
    const uint32_t K = 0x13578642u; uint32_t a = K;
    for (int i = 0; i < 0x1200; i++) { w[i] ^= a; a ^= a << 1; sz[i] ^= (uint8_t)a; a = ~a ^ K; }
}
```
The C version is not compiled here; the Python equivalent is exercised by `tests/slz/check_local.py` (needs the local discs).
