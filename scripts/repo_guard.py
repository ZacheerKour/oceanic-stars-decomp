"""Fails if a tracked file looks like game data or a privacy leak. Needs no game data. Usage: python -I scripts/repo_guard.py"""
import re, subprocess, sys, os, hashlib
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
files = subprocess.check_output(["git", "-C", root, "ls-files", "-z"]).decode("utf-8").split("\0")[:-1]
BAD_EXT = {".bin", ".cue", ".iso", ".chd", ".7z", ".exe", ".dll", ".mcd", ".img", ".mdf", ".mds", ".ccd", ".sub", ".sav", ".state"}
BAD_DIR = (".beads/", ".claude/", ".codex/", ".agents/")
WINUSER = re.compile(r"[A-Za-z]:[\\/]+Users[\\/]+([^\\/\s'\"`<>*%$:;,)\]]+)", re.I)
OK_USER = {"<home>", "public", "default", "all users", "<name>", "<user>", "username", "%username%", "$env", "user"}
MAIL = re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+")
CTRL = re.compile(rb"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]")
bad = []
for f in files:
    p = os.path.join(root, f)
    if not os.path.isfile(p): continue
    low = f.lower()
    if f == "INSTALL.exe":  # the one tracked binary: our own bootstrap, no game code; must match the hash committed next to its source
        want = open(os.path.join(root, "recomp/installer/INSTALL.exe.sha256")).read().strip()
        if hashlib.sha256(open(p, "rb").read()).hexdigest() != want: bad.append((f, "does not match recomp/installer/INSTALL.exe.sha256"))
        continue
    if os.path.splitext(low)[1] in BAD_EXT: bad.append((f, "game-like binary extension"))
    if low.startswith(BAD_DIR): bad.append((f, "internal tooling directory"))
    if os.path.getsize(p) > 5 * 1024 * 1024: bad.append((f, "over 5 MB"))
    data = open(p, "rb").read()
    # vectors: synthetic binaries from tests/slz/gen_vectors.py, exempt from the control-character check
    if not f.startswith("tests/slz/vectors/") and CTRL.search(data): bad.append((f, "control character"))
    if b"\0" in data[:8192]: continue  # binary: text checks below do not apply
    t = data.decode("utf-8", "replace")
    for m in WINUSER.finditer(t):
        if m.group(1).lower() not in OK_USER: bad.append((f, "Windows user path: " + m.group(0)[:40]))
    for m in MAIL.finditer(t):
        if not m.group().lower().endswith("@users.noreply.github.com"): bad.append((f, "e-mail address: " + m.group()))
for f, why in bad: print("FAIL", f, why)
print("repo_guard: %d file(s) checked, %d problem(s)" % (len(files), len(bad)))
sys.exit(1 if bad else 0)
