"""Function-entry verification for seed lists (used by make_seeds.py and classify_seeds.py).  Addresses only, no game bytes stored.
A candidate entry is real code when a forward walk from it decodes only valid R3000 words (no COP1/COP3, no reserved
SPECIAL/REGIMM, no branch out of the module) and ends at a `jr ra` (or a tail `j`) once every forward branch target is passed.
Data decoded as code hits a reserved opcode or runs off the module almost at once.  An entry is accepted when
  called   : a jal inside already-accepted code targets it (so a jal word in data never seeds anything), or
  prologue : it has an `addiu sp,sp,-N` within its first 16 words, or
  trusted  : the caller says so (byte-matched, hand-checked, high-confidence indirect row)."""
import struct

_SPECIAL = {0, 2, 3, 4, 6, 7, 8, 9, 12, 13, 16, 17, 18, 19, 24, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39, 42, 43}
_OPS = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 32, 33, 34, 35, 36, 37, 38, 40, 41, 42, 43, 46, 48, 50, 56, 58, 18}


def valid(x):
    op = x >> 26
    if op == 0: return (x & 0x3f) in _SPECIAL
    if op == 1: return (x >> 16) & 0x1f in (0, 1, 16, 17)
    if op == 16: return (x >> 21) & 0x1f in (0, 4, 16)
    return op in _OPS


def words(b):
    return struct.unpack('<%dI' % (len(b) // 4), b[:len(b) // 4 * 4])


def walk(w, base, a, limit=20000):
    """-> (set of word indexes, set of jal targets, has_prologue) or None when the bytes at `a` are not a complete function."""
    n, i = len(w), (a - base) >> 2
    if a % 4 or not 0 <= i < n: return None
    first, maxt, idx, jals, pro = i, i, [], set(), False
    while len(idx) < limit:
        if i >= n or not valid(w[i]): return None
        x, op = w[i], w[i] >> 26
        idx.append(i)
        if i - first < 16 and x >> 16 == 0x27bd and x & 0x8000: pro = True
        end = False
        if op in (1, 4, 5, 6, 7):
            t = i + 1 + ((x & 0xffff) ^ 0x8000) - 0x8000
            if not 0 <= t < n: return None
            maxt = max(maxt, t)
        elif op == 3: jals.add(((x & 0x3ffffff) << 2) | 0x80000000)
        elif op == 2:
            t = ((((x & 0x3ffffff) << 2) | 0x80000000) - base) >> 2
            if i < t <= i + 256 and t < n: maxt = max(maxt, t)
            elif i + 1 >= maxt: end = True                  # tail call out of the function
        elif op == 0 and x & 0x3f == 8 and (x >> 21) & 0x1f == 31 and i + 1 >= maxt: end = True
        i += 1
        if end:
            if i >= n or not valid(w[i]): return None
            idx.append(i); return set(idx), jals, pro
    return None


def verify(w, base, roots, trusted=()):
    """roots: candidate entries needing a prologue; trusted: entries accepted on walk-validity alone (kept even if the walk fails).
    -> (entries, found = entries accepted by walk + prologue/caller rather than on trust, jal targets seen in accepted code, rejected {addr: reason})."""
    top = base + 4 * len(w)
    entries, found, allj, bad = set(), set(), set(), {}
    queue = [(a, 'trusted') for a in trusted] + [(a, 'prologue') for a in roots]
    while queue:
        a, why = queue.pop()
        if a in found: continue
        r = walk(w, base, a) if base <= a < top else None
        if r is None:
            if why == 'trusted': entries.add(a)
            bad.setdefault(a, 'not code (invalid opcode / no end / off module)'); continue
        if why == 'prologue' and not r[2]:
            bad.setdefault(a, 'no prologue and no verified caller'); continue
        bad.pop(a, None); entries.add(a); allj |= r[1]
        if why != 'trusted': found.add(a)
        queue.extend((t, 'called') for t in r[1] if base <= t < top and t % 4 == 0 and t not in found)
    return entries, found, allj, bad
