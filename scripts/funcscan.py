"""Function inventory of the main overlay (entry 1) from the local build/main.bin (gitignored; run `python -I scripts/funcscan.py`).
Prints addr, size, instruction count, calls, callers, and flags: leaf, loop (backward branch), jtab (jr non-ra),
gte (cop2/lwc2/swc2), fpu (cop1), muldiv, bad (undefined opcodes: probably data). Output has no game bytes.
Boundaries: start at the image start, jal targets and the word after a function end; end at the first `jr ra`/`j` out whose
delay slot lies past the farthest forward branch seen. Sizes include the delay slot, exclude trailing padding nops."""
import struct, sys, os
BASE = 0x8002f810
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
data = open(os.path.join(root, "build", "main.bin"), "rb").read()
LIMIT = int(sys.argv[1], 0) if len(sys.argv) > 1 else len(data)
W = struct.unpack("<%dI" % (LIMIT // 4), data[:LIMIT])
N = len(W)
VALID_OPS = set(range(0, 12)) | {0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x12} | set(range(0x20, 0x2f)) | {0x30, 0x32, 0x38, 0x3a}
def sx16(w): return (w & 0xffff) - 0x10000 if w & 0x8000 else w & 0xffff
def info(i):
    w = W[i]; op = w >> 26; f = {}
    if op == 3: f["jal"] = (w & 0x3ffffff) << 2 | 0x80000000
    elif op == 2: f["j"] = (w & 0x3ffffff) << 2 | 0x80000000
    elif op == 0 and (w & 0x3f) == 8: f["jr"] = (w >> 21) & 31
    elif op == 0 and (w & 0x3f) == 9: f["jalr"] = 1
    elif op in (4, 5, 6, 7) or (op == 1 and ((w >> 16) & 0x1f) in (0, 1)): f["br"] = i + 1 + sx16(w)
    elif op == 1 and ((w >> 16) & 0x1f) in (16, 17): f["jal"] = None; f["br"] = i + 1 + sx16(w)  # bltzal/bgezal
    if op == 0 and (w & 0x3f) in (0x18, 0x19, 0x1a, 0x1b): f["muldiv"] = 1
    if op in (0x12, 0x32, 0x3a): f["gte"] = 1
    if op in (0x11, 0x31, 0x39, 0x13): f["fpu"] = 1
    if op == 0x11 or op not in VALID_OPS and op not in (0x31, 0x39, 0x13): f["bad"] = 1
    if op == 0 and (w & 0x3f) not in (0,2,3,4,6,7,8,9,12,13,16,17,18,19,24,25,26,27,32,33,34,35,36,37,38,39,42,43): f["bad"] = 1
    return f
starts = {0}
for i in range(N):
    j = info(i).get("jal")
    if j and BASE <= j < BASE + LIMIT: starts.add((j - BASE) // 4)
funcs = []; i = 0
while i < N:
    while i < N and W[i] == 0 and i not in starts: i += 1   # padding nops
    if i >= N: break
    s = i; far = s; k = s
    while k < N:
        f = info(k)
        if "br" in f and f["br"] > far: far = f["br"]
        if (("jr" in f and f["jr"] == 31) or ("j" in f and not (BASE + s * 4 <= f["j"] < BASE + k * 4 + 8))) and k + 1 >= far:
            k += 1; break
        k += 1
        if k > s and k in starts and k > far: break
    e = k + 1
    funcs.append((s, min(e, N)))
    i = e
callers = {}
rows = []
for s, e in funcs:
    calls = []; fl = set(); loop = False
    for k in range(s, e):
        f = info(k)
        if f.get("jal"): calls.append(f["jal"]); callers.setdefault(f["jal"], 0); callers[f["jal"]] += 1
        if "jalr" in f: calls.append(0)
        if "br" in f and f["br"] <= k and s <= f["br"]: loop = True
        if "j" in f and BASE + s * 4 <= f["j"] <= BASE + k * 4: loop = True
        if "jr" in f and f["jr"] != 31: fl.add("jtab")
        for t in ("gte", "fpu", "muldiv", "bad"):
            if t in f: fl.add(t)
    if loop: fl.add("loop")
    if not calls: fl.add("leaf")
    rows.append((BASE + s * 4, (e - s) * 4, e - s, len(calls), fl))
if __name__ == "__main__":
    print("addr\tsize\tinstr\tcalls\tcallers\tflags")
    for a, sz, n, c, fl in rows:
        print("%08X\t%d\t%d\t%d\t%d\t%s" % (a, sz, n, c, callers.get(a, 0), ",".join(sorted(fl))))
