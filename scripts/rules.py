"""Mechanical checks for the cheaply checkable decomp-code-rules (see AGENTS.md, "Project profile").
Usage: python -I scripts/rules.py [files...]    default: src/*.c src/*.h (src/wip is parked and not checked)
FAIL = [match] rule (the function would be MATCH-PENDING); WARN = [free]/[checked] hygiene. Exit 1 if any FAIL.
Not checkable here (reader test, review): R17 dummy temporaries, merged variables, invented names, quirk labels."""
import re, sys, glob, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FN = re.compile(r"^func_[0-9A-F]{8}$"); GL = re.compile(r"^D_[0-9A-F]{8}$")
out = []
def say(sev, rule, f, line, msg): out.append((sev, rule, os.path.relpath(f, root).replace("\\", "/"), line, msg))
def strip(t):  # comments -> blanks, keeps line numbers
    return re.sub(r"/\*.*?\*/|//[^\n]*", lambda m: re.sub(r"[^\n]", " ", m.group()), t, flags=re.S)
def items(t):  # file-scope items: (head text without body, has_body, first line)
    res, depth, cur, line, start, body = [], 0, "", 1, 1, False
    for ch in t:
        if ch == chr(10): line += 1
        if depth == 0 and not cur.strip() and not ch.isspace(): start = line
        if ch == "{":
            body = True if depth == 0 else body; depth += 1
            if depth == 1: continue
        elif ch == "}":
            depth -= 1
            if depth == 0 and not re.match(r"\s*(typedef|struct|union|enum)", cur):  # function body ends the item
                res.append((cur.strip(), True, start)); cur, body = "", False
            continue
        if depth == 0:
            if ch == ";": res.append((cur.strip(), body, start)); cur, body = "", False
            else: cur += ch
    return res
decl = {}  # symbol -> first place, for duplicate detection
def names(item):  # identifiers declared by an extern/prototype item
    item = re.sub(r"^\s*extern\s+", "", item)
    return re.findall(r"\*?\s*(\b[A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:\([^)]*\))?\s*(?:,|$)", item.split("=")[0])
def check(f):
    raw = open(f, encoding="utf-8", errors="replace").read(); t = strip(raw)
    is_c = f.endswith(".c"); stem = os.path.splitext(os.path.basename(f))[0]
    for n, l in enumerate(t.split("\n"), 1):
        if re.search(r"\b(__asm__|__asm|asm)\b", l): say("FAIL", "R18", f, n, "inline assembly")
        if re.match(r"\s*#\s*pragma", l): say("FAIL", "R17", f, n, "#pragma")
        if re.search(r"\b0x8[0-9A-Fa-f]{7}\b", l) and not re.match(r"\s*#", l): say("FAIL", "R4", f, n, "absolute address literal in code (use a declared D_<ADDR>/func_<ADDR>)")
        if re.search(r"\*\s*\(\s*[A-Za-z_][\w\s]*\*+\s*\)\s*\(|\(\s*(?:unsigned\s+)?(?:char|void)\s*\*\s*\)[^;]*[-+]", l): say("FAIL", "R4", f, n, "offset/byte-pointer cast arithmetic")
        if re.search(r"\bvolatile\b", l): say("WARN", "R17", f, n, "volatile (hardware access only; else a carrier fakematch)")
        if re.search(r"\bpad\w*\s*\[", l): say("WARN", "R26", f, n, "padding member (struct layout not recovered)")
    for n, l in enumerate(raw.split("\n"), 1):
        if len(l) > 120: say("WARN", "R23", f, n, "line > 120 columns")
    if is_c and not re.search(r'^\s*#\s*include\s+"[^"]+\.h"', raw, re.M): say("WARN", "R2", f, 1, "no project header included")
    defs = []
    for text, body, ln in items(re.sub(r"^\s*#[^\n]*", "", t, flags=re.M)):
        if not text: continue
        if re.match(r"typedef\b|(struct|union|enum)\b", text):
            if is_c: say("FAIL", "R2", f, ln, "type declared in a .c file (local copy; put types in a header)")
        elif body and "(" in text:
            m = re.search(r"(\w+)\s*\([^()]*\)\s*$", text.replace("{", ""))
            nm = m.group(1) if m else "?"; defs.append(nm)
            if not FN.match(nm): say("FAIL", "R7", f, ln, "function %s is not func_<ADDR>" % nm)
            elif is_c and nm != stem: say("WARN", "R12", f, ln, "function %s defined in %s.c" % (nm, stem))
        elif re.match(r"extern\b", text) or "(" in text:
            if is_c: say("FAIL", "R1", f, ln, "extern/prototype in a .c file (declare once in a header)")
            for nm in names(text):
                if nm in ("void", "int", "char", "short", "long", "unsigned", "signed", "float", "double"): continue
                if not (FN.match(nm) or GL.match(nm)): say("FAIL", "R7", f, ln, "declared symbol %s is not func_<ADDR>/D_<ADDR>" % nm)
                if nm in decl and decl[nm] != (f, ln): say("FAIL", "R1", f, ln, "%s declared again (first at %s:%d)" % (nm, os.path.relpath(decl[nm][0], root), decl[nm][1]))
                decl.setdefault(nm, (f, ln))
        elif is_c: say("FAIL", "R1", f, ln, "global variable defined in a .c file: %s" % text[:40])
    if is_c and defs.count(stem) != 1: say("FAIL", "R12", f, 1, "expected exactly one definition of %s" % stem)
if __name__ == "__main__":
    files = sys.argv[1:] or sorted(glob.glob(os.path.join(root, "src", "*.h")) + glob.glob(os.path.join(root, "src", "*.c")))
    for f in files: check(f)
    for sev, rule, f, ln, msg in sorted(out, key=lambda x: (x[2], x[3])): print("%s %s %s:%d: %s" % (sev, rule, f, ln, msg))
    nf = sum(1 for o in out if o[0] == "FAIL"); bad = {o[2] for o in out if o[0] == "FAIL" and o[2].endswith(".c")}
    ncf = len([x for x in files if x.endswith(".c")])
    print("rules: %d FAIL (%d of %d .c files), %d WARN" % (nf, len(bad), ncf, len(out) - nf))
    sys.exit(1 if nf else 0)
