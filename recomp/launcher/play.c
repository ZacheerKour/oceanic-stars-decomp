/* PLAYTHISGAME.exe: the Steam-friendly launcher, with no PowerShell dependency. "Add a Non-Steam Game" lists only .exe files, and Steam
 * tracks the process it starts, so this waits for the game as a direct child.
 * prepare() is a native port of play.ps1 (kept as the PLAY.bat fallback and as the equivalence reference): it reads settings.ini,
 * writes play\game.toml, play\settings.warn, play\launch.txt, keybinds.ini and config.ini, and fills Prep for the launch.
 * Flags: --prepare (write the files, do not start the game), --selftest (assert-based), --root <dir> (default: the exe's dir).
 * Intentional differences from play.ps1: env lines come in settings.ini order (PS hashtable order is unspecified); audio.buffer_ms
 * and pad.deadzone out of range or non-numeric now fall back with a settings.warn note (PS aborted or passed them through).
 * settings.ini may be UTF-8 (BOM or not), UTF-16 (either byte order) or ANSI; it is read as UTF-8 and a note goes to settings.warn when
 * it was converted or could not be decoded. A named mutex (hash of the game folder path) lets one launcher run per folder; a second one
 * exits with code 3 and touches nothing, so a double Steam launch cannot break the running game. --quiet skips message boxes (tests).
 * All strings are UTF-8; they are widened at the Win32 boundary. Build: recomp\launcher\build.ps1 is the standalone way; the
 * installer compiles play.c with cl directly. */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* ---------- small helpers (short-lived process: nothing is freed) ---------- */
static char *fmt(const char *f, ...) {
    va_list a; va_start(a, f); int n = vsnprintf(NULL, 0, f, a); va_end(a);
    char *s = malloc(n + 1); va_start(a, f); vsnprintf(s, n + 1, f, a); va_end(a); return s;
}
typedef struct { char *p; size_t n, cap; } Sb;
static void sb_add(Sb *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0;
}
static void sb_s(Sb *b, const char *s) { sb_add(b, s, strlen(s)); }
static void sb_f(Sb *b, const char *f, ...) {
    va_list a; va_start(a, f); int n = vsnprintf(NULL, 0, f, a); va_end(a);
    char *t = malloc(n + 1); va_start(a, f); vsnprintf(t, n + 1, f, a); va_end(a); sb_add(b, t, n);
}
static wchar_t *W(const char *s) { int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0); wchar_t *w = malloc(n * 2); MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n); return w; }
static char *U(const wchar_t *w) { int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL); char *s = malloc(n); WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL); return s; }
static char *full_path(const char *p) {   /* absolute path, any length; NULL on failure */
    wchar_t *w = W(p); DWORD n = GetFullPathNameW(w, 0, NULL, NULL); if (!n) return NULL;
    wchar_t *b = malloc(n * 2); DWORD m = GetFullPathNameW(w, n, b, NULL); return (!m || m >= n) ? NULL : U(b);
}
static int exists(const char *p) { return GetFileAttributesW(W(p)) != INVALID_FILE_ATTRIBUTES; }
static char *read_file(const char *p, size_t *len) {   /* NULL if missing; a UTF-8 BOM is dropped (Get-Content does the same) */
    HANDLE f = CreateFileW(W(p), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD n = GetFileSize(f, NULL), got = 0; char *b = malloc(n + 1);
    if (!ReadFile(f, b, n, &got, NULL)) got = 0;
    CloseHandle(f); b[got] = 0;
    if (got >= 3 && !memcmp(b, "\xEF\xBB\xBF", 3)) { memmove(b, b + 3, got - 2); got -= 3; }
    if (len) *len = got; return b;
}
static int write_str(const char *p, const char *s) {   /* UTF-8, no BOM, like [IO.File]::WriteAllText */
    HANDLE f = CreateFileW(W(p), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD got, n = (DWORD)strlen(s); int ok = WriteFile(f, s, n, &got, NULL) && got == n; CloseHandle(f); return ok;
}
static char *ini_decode(char *b, size_t n, Sb *w) {   /* settings.ini text -> UTF-8; notes go to w */
    unsigned char *u = (unsigned char *)b;
    if (n >= 2 && ((u[0] == 0xFF && u[1] == 0xFE) || (u[0] == 0xFE && u[1] == 0xFF))) {   /* UTF-16 with a BOM */
        int be = u[0] == 0xFE; size_t wn = (n - 2) / 2; wchar_t *ws = malloc((wn + 1) * 2);
        for (size_t i = 0; i < wn; i++) ws[i] = be ? (u[2 + 2 * i] << 8 | u[3 + 2 * i]) : (u[3 + 2 * i] << 8 | u[2 + 2 * i]);
        int m = wn ? WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, (int)wn, NULL, 0, NULL, NULL) : 0;
        if ((n - 2) % 2 || (wn && !m)) { sb_s(w, "settings.ini is UTF-16 but cannot be decoded; using the defaults. Save it as UTF-8.\n"); return _strdup(""); }
        char *r = malloc(m + 1); if (m) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, (int)wn, r, m, NULL, NULL); r[m] = 0; return r;
    }
    if (n && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b, (int)n, NULL, 0)) {   /* not UTF-8: read it as the ANSI code page */
        int wn = MultiByteToWideChar(CP_ACP, 0, b, (int)n, NULL, 0); wchar_t *ws = malloc((wn + 1) * 2);
        MultiByteToWideChar(CP_ACP, 0, b, (int)n, ws, wn); ws[wn] = 0;
        sb_s(w, "settings.ini is not valid UTF-8; read as ANSI. Save it as UTF-8 if a path or key looks wrong.\n"); return U(ws);
    }
    return b;
}
static void mkdirs(const char *p) {
    char *s = _strdup(p);
    for (char *c = s + 1; ; c++) {
        if (*c == '\\' || *c == '/' || !*c) { char t = *c; *c = 0; CreateDirectoryW(W(s), NULL); if (!t) break; *c = t; }
    }
}
static char *parent(const char *p) { char *s = _strdup(p); char *c = strrchr(s, '\\'); if (c) *c = 0; return s; }
static char *slashes(const char *p) { char *s = _strdup(p); for (char *c = s; *c; c++) if (*c == '\\') *c = '/'; return s; }
static char *lower(const char *s) { char *r = _strdup(s); for (char *c = r; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; return r; }
static char *replace_all(const char *src, const char *from, const char *to) {
    Sb b = { 0 }; size_t fl = strlen(from); const char *c = src, *m;
    while ((m = strstr(c, from))) { sb_add(&b, c, m - c); sb_s(&b, to); c = m + fl; }
    sb_s(&b, c); return b.p;
}

/* ---------- settings.ini: key = value, keys case-insensitive, last one wins (a PowerShell hashtable, as in play.ps1) ---------- */
#define MAXSET 512
static char *sk[MAXSET], *sv[MAXSET]; static int sn;
static int isw(int c) { return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int isws(int c) { return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n'; }
static const char *get(const char *k) { for (int i = 0; i < sn; i++) if (!_stricmp(sk[i], k)) return sv[i]; return ""; }
static void put(const char *k, const char *v) {
    for (int i = 0; i < sn; i++) if (!_stricmp(sk[i], k)) { sv[i] = _strdup(v); return; }
    if (sn < MAXSET) { sk[sn] = _strdup(k); sv[sn++] = _strdup(v); }
}
static void parse_ini(const char *t) {   /* ^\s*([A-Za-z_][\w.]*)\s*=\s*(.*?)\s*$ per line (\n, \r\n or \r ends a line) */
    sn = 0;
    while (*t) {
        const char *e = t; while (*e && *e != '\n' && *e != '\r') e++;
        const char *c = t; while (c < e && isws(*c)) c++;
        if (c < e && isw(*c) && !(*c >= '0' && *c <= '9')) {
            const char *ks = c; while (c < e && (isw(*c) || *c == '.')) c++;
            const char *ke = c; while (c < e && isws(*c)) c++;
            if (c < e && *c == '=') {
                c++; while (c < e && isws(*c)) c++;
                const char *ve = e; while (ve > c && isws(ve[-1])) ve--;
                put(fmt("%.*s", (int)(ke - ks), ks), fmt("%.*s", (int)(ve - c), c));
            }
        }
        t = e; if (*t == '\r' && t[1] == '\n') t++; if (*t) t++;
    }
}

/* ---------- validated values: a bad or missing one falls back to the default and leaves a note in settings.warn ---------- */
static Sb warn;
static int all_digits(const char *v, int maxlen) { int n = (int)strlen(v); if (!n || n > maxlen) return 0; for (; *v; v++) if (*v < '0' || *v > '9') return 0; return 1; }
static int ok_width(const char *v) { return all_digits(v, 4) && v[0] != '0' && atoi(v) >= 640 && atoi(v) <= 7680; }   /* the runtime's range */
static int ok_scale(const char *v) { return all_digits(v, 1) && v[0] >= '1' && v[0] <= '8'; }
static int ok_filter(const char *v) { return !strcmp(v, "linear") || !strcmp(v, "nearest"); }
static int ok_onoff(const char *v) { return !strcmp(v, "on") || !strcmp(v, "off"); }
static int ok_wmode(const char *v) { return !strcmp(v, "windowed") || !strcmp(v, "borderless"); }
static int ok_mode(const char *v) { return !strcmp(v, "fit") || !strcmp(v, "integer") || !strcmp(v, "stretch"); }
static int ok_audio(const char *v) { return all_digits(v, 6) && atoi(v) >= 30 && atoi(v) <= 500; }   /* the runtime rejects values outside this */
static int ok_dz(const char *v) { return all_digits(v, 6) && atoi(v) <= 32767; }
static char *pick(const char *k, const char *def, int (*ok)(const char *)) {
    char *v = lower(get(k)); if (!*v) return _strdup(def);
    if (ok(v)) return v;
    sb_f(&warn, "settings.ini %s = %s is not valid; using %s\n", k, get(k), def); return _strdup(def);
}

/* ---------- prepare: the port of play.ps1 ---------- */
typedef struct { char *exe, *args, *run; char *env[MAXSET + 2]; int nenv; } Prep;
static char errmsg[1024];

static int fail(const char *root, const char *m) {   /* also leaves play\prepare.err, as play.ps1 -Prepare does */
    char *P = fmt("%s\\play", root); mkdirs(P);
    write_str(fmt("%s\\prepare.err", P), m); snprintf(errmsg, sizeof errmsg, "%s", m); return 0;
}
static const char *DISC = "Star Ocean - The Second Story (USA) (Disc %d)";

static int prepare(const char *root, Prep *o) {
    memset(o, 0, sizeof *o); sn = 0; memset(&warn, 0, sizeof warn);
    char *exedir = fmt("%s\\recomp\\rtfull\\_build_final", root), *exe = fmt("%s\\psxrecomp-game.exe", exedir);
    char *psx = fmt("%s\\tools\\ref\\psxrecomp", root), *P = fmt("%s\\play", root), *S = fmt("%s\\saves", root);
    if (!exists(exe)) return fail(root, "the game is not built yet: run INSTALL.bat first");
    char *cue[2];
    for (int n = 1; n <= 2; n++) {
        char *c = fmt("%s\\roms\\disc%d\\%s.cue", root, n, fmt(DISC, n));
        if (!exists(c)) return fail(root, fmt("missing %s: run INSTALL.bat with your discs", c));
        cue[n - 1] = slashes(c);
    }
    char *ini = fmt("%s\\settings.ini", root);
    if (!exists(ini) && !CopyFileW(W(fmt("%s\\settings.default.ini", root)), W(ini), TRUE)) return fail(root, "settings.default.ini is missing: re-download the game folder");
    size_t iln; char *it = read_file(ini, &iln); if (!it) return fail(root, "cannot read settings.ini");
    parse_ini(ini_decode(it, iln, &warn));

    mkdirs(P); mkdirs(S); mkdirs(fmt("%s\\bios", exedir));
    char *bios = _strdup(get("bios")); if (!*bios) bios = _strdup("auto");
    char *own = fmt("%s\\roms\\scph5501.bin", root);
    if (!_stricmp(bios, "auto"))   /* own dump only when the build linked its backend (build_all saw it) */
        bios = (exists(own) && exists(fmt("%s\\recomp\\bios\\_gen\\generated\\SCPH5501_full.c", root))) ? own : _strdup("openbios");
    char *t = read_file(fmt("%s\\recomp\\game.toml", root), NULL); if (!t) return fail(root, "recomp\\game.toml is missing");
    for (int n = 1; n <= 2; n++) t = replace_all(t, fmt("\"disc/%s.cue\"", fmt(DISC, n)), fmt("\"%s\"", cue[n - 1]));
    t = replace_all(t, "exe = \"disc/SCUS_944.21\"", fmt("exe = \"%s\"", slashes(fmt("%s\\recomp\\host\\_out\\proj\\disc\\SCUS_944.21", root))));
    if (!_stricmp(bios, "openbios")) {
        Sb b = { 0 }; const char *c = t, *m;   /* (?m)^openbios = false -> "openbios = true " (game.toml pins retail; OpenBIOS must be allowed) */
        while ((m = strstr(c, "openbios = false")) && (m == t || m[-1] == '\n')) { sb_add(&b, c, m - c); sb_s(&b, "openbios = true "); c = m + 16; }
        /* shortcut: only line-start matches are replaced and a mid-line hit stops the scan; fine for the pinned game.toml */
        sb_s(&b, c); t = b.p;
        bios = fmt("%s\\bios\\openbios.bin", exedir);
        if (!CopyFileW(W(fmt("%s\\bios\\openbios.bin", psx)), W(bios), FALSE)) return fail(root, "the bundled OpenBIOS is missing: run INSTALL.bat first");
    } else if (!exists(bios)) return fail(root, fmt("settings.ini bios = %s does not exist", bios));

    char *dzs = pick("pad.deadzone", "9000", ok_dz), *abs_ = pick("audio.buffer_ms", "180", ok_audio);
    Sb tb = { 0 }; sb_s(&tb, t);
    sb_f(&tb, "\n[controller]\np1_device = \"auto\"\np1_mode = \"digital\"\ndeadzone = %d\n", atoi(dzs));
    sb_f(&tb, "\n[audio]\nbuffer_ms = %d\n", atoi(abs_));
    char *ww = pick("window_width", "960", ok_width), *rs = pick("render_scale", "1", ok_scale), *flt = pick("filter", "linear", ok_filter);
    put("env.PSXRT_SCALE_MODE", pick("scale_mode", "fit", ok_mode));   /* env.* lines reach the game */
    /* in-game Graphics options (stubs/psxrt_io.c): scanlines and window mode apply at start, the menu writes every change back to
       settings.ini (PSXRT_SETTINGS_INI), PSXRT_WINDOW_WIDTH seeds the size row while the game starts fullscreen */
    put("env.PSX_SCANLINES", strcmp(pick("scanlines", "off", ok_onoff), "on") ? "0" : "1");
    put("env.PSXRT_FULLSCREEN", strcmp(pick("window_mode", "windowed", ok_wmode), "borderless") ? "0" : "1");
    put("env.PSXRT_WINDOW_WIDTH", ww);
    put("env.PSXRT_SETTINGS_INI", ini);
    if (strcmp(get("hint_shown"), "1")) put("env.PSXRT_HINT", "1");   /* first-start toast, the game then writes hint_shown = 1 */
    {   /* (?m)^\[video\].*$ -> $0 + the three keys; .* takes a CR too, so the inserted text lands after it */
        Sb b = { 0 }; const char *c = tb.p;
        while (*c) {
            const char *e = strchr(c, '\n'); size_t ln = e ? (size_t)(e - c) : strlen(c);
            sb_add(&b, c, ln);
            if (ln >= 7 && !strncmp(c, "[video]", 7))
                sb_f(&b, "\nwindow_width = %s\nsupersampling = %s\nantialiasing = %s", ww, rs, strcmp(flt, "linear") ? "false" : "true");
            if (!e) break; sb_add(&b, "\n", 1); c = e + 1;
        }
        t = b.p;
    }
    write_str(fmt("%s\\game.toml", P), t);
    write_str(fmt("%s\\settings.warn", P), warn.p ? warn.p : "");

    /* Keyboard: key.<button> = <key>[, <alt key>]; defaults below. Rewritten into keybinds.ini each start. */
    static const char *keys[][2] = { { "up", "W, Up" }, { "down", "S, Down" }, { "left", "A, Left" }, { "right", "D, Right" },
        { "cross", "Space, 1" }, { "circle", "Q, 2" }, { "square", "E, 3" }, { "triangle", "R, 4" },
        { "l1", "Left Shift, Z" }, { "r1", "F, X" }, { "l2", "Tab, C" }, { "r2", "G, V" }, { "l3", "T" }, { "r3", "Y" },
        { "start", "Escape, Return" }, { "select", "Backspace, Right Shift" } };
    Sb kb = { 0 }; sb_s(&kb, "; written by play.ps1 from settings.ini (key.* lines); edit settings.ini, not this file\n[player1]\n");
    for (int i = 0; i < 16; i++) { const char *v = get(fmt("key.%s", keys[i][0])); if (!*v) v = keys[i][1]; sb_f(&kb, "%s = %s\n", keys[i][0], v); }
    static const char *ls[] = { "ls_up", "ls_down", "ls_left", "ls_right", "rs_up", "rs_down", "rs_left", "rs_right" };
    for (int i = 0; i < 8; i++) sb_f(&kb, "%s = None\n", ls[i]);
    write_str(fmt("%s\\keybinds.ini", exedir), kb.p);
    /* Host hotkeys (runtime config.ini [KeyMap]): F8 state browser, Turbo (hold to fast-forward) = F2; F10 pause menu is built in. */
    write_str(fmt("%s\\config.ini", exedir), "; written by play.ps1\n[KeyMap]\nSaveStateMenu = F8\nFullscreen = Alt+Return, Ctrl+F, F11\nScanlines = F4\nTurboToggle = F3\nTurbo = F2\n");

    /* Odd gamepads: gamecontrollerdb.txt next to PLAYTHISGAME.exe or the game exe */
    const char *dirs[2] = { root, exedir };
    for (int i = 0; i < 2; i++) { char *f = fmt("%s\\gamecontrollerdb.txt", dirs[i]);
        if (exists(f) && !GetEnvironmentVariableW(L"SDL_GAMECONTROLLERCONFIG_FILE", NULL, 0)) SetEnvironmentVariableW(L"SDL_GAMECONTROLLERCONFIG_FILE", W(f)); }

    char *fbios = full_path(bios); if (!fbios) return fail(root, fmt("cannot resolve the BIOS path %s", bios));
    Sb a = { 0 }; sb_f(&a, "\"--no-launcher\" \"--game\" \"%s\\game.toml\" \"--memcard-dir\" \"%s\" \"--bios\" \"%s\"", P, S, fbios);
    const char *x = get("extra_args");
    while (*x) {   /* -split '\s+' */
        while (isws(*x)) x++; if (!*x) break;
        const char *e = x; while (*e && !isws(*e)) e++;
        sb_f(&a, " \"%.*s\"", (int)(e - x), x); x = e;
    }
    o->exe = exe; o->args = a.p; o->run = P;
    for (int i = 0; i < sn; i++) if (!_strnicmp(sk[i], "env.", 4) && sk[i][4]) o->env[o->nenv++] = fmt("%s=%s", sk[i] + 4, sv[i]);
    DWORD sdn = GetEnvironmentVariableW(L"SDL_GAMECONTROLLERCONFIG_FILE", NULL, 0); wchar_t *sd = malloc((sdn + 1) * 2);
    if (sdn && GetEnvironmentVariableW(L"SDL_GAMECONTROLLERCONFIG_FILE", sd, sdn)) o->env[o->nenv++] = fmt("SDL_GAMECONTROLLERCONFIG_FILE=%s", U(sd));
    if (strlen(exe) + strlen(a.p) + 8 > 32767) return fail(root, "the launch command line is too long (a long extra_args in settings.ini or a very deep game folder)");
    Sb lt = { 0 }; sb_f(&lt, "%s\n%s\n%s\n", o->exe, o->args, o->run);
    for (int i = 0; i < o->nenv; i++) sb_f(&lt, "%s\n", o->env[i]);
    write_str(fmt("%s\\launch.txt", P), lt.p);   /* kept for debugging and the equivalence diff; the launch below uses Prep directly */
    return 1;
}

/* ---------- single instance, per game folder ---------- */
static int quiet;
static void die(const char *m) { if (!quiet) MessageBoxW(NULL, W(m), L"Star Ocean: not ready", MB_OK | MB_ICONERROR); fprintf(stderr, "PLAY: %s\n", m); ExitProcess(1); }
static HANDLE take_lock(const char *root) {   /* NULL when another launcher already holds this folder (the handle is kept until exit) */
    char *k = lower(root); size_t n = strlen(k); while (n > 3 && k[n - 1] == '\\') k[--n] = 0;
    unsigned long long h = 1469598103934665603ULL; for (char *c = k; *c; c++) h = (h ^ (unsigned char)*c) * 1099511628211ULL;
    HANDLE m = CreateMutexW(NULL, FALSE, W(fmt("Local\\StarOcean2Play_%016llx", h)));
    if (m && GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(m); return NULL; }
    return m;
}

/* ---------- self-test ---------- */
static int tfail;
#define CHECK(c) do { if (!(c)) { tfail++; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); } } while (0)
static void mk(const char *root, const char *rel, const char *data) { char *p = fmt("%s\\%s", root, rel); mkdirs(parent(p)); write_str(p, data); }
static int has(const char *root, const char *rel, const char *needle) { char *d = read_file(fmt("%s\\%s", root, rel), NULL); return d && strstr(d, needle) != NULL; }
static void raw(const char *root, const char *b, size_t n) {   /* settings.ini from raw bytes */
    char *p = fmt("%s\\settings.ini", root); HANDLE f = CreateFileW(W(p), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD got;
    WriteFile(f, b, (DWORD)n, &got, NULL); CloseHandle(f);
}
static void tree16(const char *root, const char *utf8, int be) {   /* settings.ini as UTF-16 with a BOM */
    wchar_t *w = W(utf8); size_t n = wcslen(w); char *b = malloc(2 + n * 2); b[0] = be ? '\xFE' : '\xFF'; b[1] = be ? '\xFF' : '\xFE';
    for (size_t i = 0; i < n; i++) { b[2 + 2 * i] = (char)(be ? w[i] >> 8 : w[i]); b[3 + 2 * i] = (char)(be ? w[i] : w[i] >> 8); }
    raw(root, b, 2 + n * 2);
}
static void tree(const char *root, const char *settings) {
    mk(root, "recomp\\rtfull\\_build_final\\psxrecomp-game.exe", "x");
    mk(root, "roms\\disc1\\Star Ocean - The Second Story (USA) (Disc 1).cue", "x");
    mk(root, "roms\\disc2\\Star Ocean - The Second Story (USA) (Disc 2).cue", "x");
    mk(root, "tools\\ref\\psxrecomp\\bios\\openbios.bin", "bios");
    mk(root, "recomp\\game.toml", "exe = \"disc/SCUS_944.21\"\ndisc = \"disc/Star Ocean - The Second Story (USA) (Disc 1).cue\"\ndiscs = [\"disc/Star Ocean - The Second Story (USA) (Disc 1).cue\", \"disc/Star Ocean - The Second Story (USA) (Disc 2).cue\"]\nopenbios = false\n[video]\n");
    mk(root, "settings.default.ini", "bios = auto\n");
    mk(root, "settings.ini", settings);
}
static int selftest(void) {
    wchar_t tmp[MAX_PATH]; DWORD tn = GetTempPathW(MAX_PATH, tmp); if (!tn || tn >= MAX_PATH) { fprintf(stderr, "selftest: temp path unusable\n"); return 1; }
    char *root = fmt("%sso2_playexe_selftest", U(tmp)); Prep o;   /* fixed dir, overwritten each run (nothing is deleted) */
    tree(root, "; c\n[play]\n  BIOS = OpenBIOS \nwindow_width = 99\nrender_scale = 4\nFILTER = Nearest\nscale_mode = zoom\naudio.buffer_ms = 9999\npad.deadzone = 123\nkey.cross = K, J\nextra_args = -a  -b\nenv.PSX_VSYNC = 1\n");
    CHECK(prepare(root, &o));
    CHECK(has(root, "play\\game.toml", "window_width = 960\nsupersampling = 4\nantialiasing = false"));   /* 99 bad -> 960 */
    CHECK(has(root, "play\\game.toml", "openbios = true \n"));
    CHECK(has(root, "play\\game.toml", "buffer_ms = 180\n"));
    CHECK(has(root, "play\\game.toml", "deadzone = 123\n"));
    CHECK(has(root, "play\\game.toml", "/roms/disc1/Star Ocean"));
    CHECK(has(root, "play\\settings.warn", "window_width = 99 is not valid; using 960\n"));
    CHECK(has(root, "play\\settings.warn", "scale_mode = zoom is not valid; using fit\n"));
    CHECK(has(root, "play\\settings.warn", "audio.buffer_ms = 9999 is not valid; using 180\n"));
    CHECK(!has(root, "play\\settings.warn", "render_scale"));
    CHECK(has(root, "recomp\\rtfull\\_build_final\\keybinds.ini", "cross = K, J\n"));
    CHECK(has(root, "recomp\\rtfull\\_build_final\\keybinds.ini", "select = Backspace, Right Shift\nls_up = None\n"));
    CHECK(has(root, "recomp\\rtfull\\_build_final\\config.ini", "Turbo = F2\n"));
    CHECK(has(root, "play\\launch.txt", "\"-a\" \"-b\"\n"));
    CHECK(has(root, "play\\launch.txt", "PSX_VSYNC=1\nPSXRT_SCALE_MODE=fit\n"));
    CHECK(o.nenv >= 2 && !strcmp(o.env[0], "PSX_VSYNC=1"));
    tree(root, "window_width = 640\nrender_scale = 8\nbios = nowhere.bin\n");
    CHECK(!prepare(root, &o) && strstr(errmsg, "nowhere.bin does not exist"));
    CHECK(has(root, "play\\prepare.err", "does not exist"));
    tree(root, "window_width = 7681\nfilter = LINEAR\n");
    CHECK(prepare(root, &o));
    CHECK(has(root, "play\\game.toml", "window_width = 960") && has(root, "play\\game.toml", "antialiasing = true"));
    tree(root, "window_width = 7680\n");
    CHECK(prepare(root, &o) && has(root, "play\\game.toml", "window_width = 7680\n"));
    CHECK(has(root, "play\\launch.txt", "PSX_SCANLINES=0\nPSXRT_FULLSCREEN=0\nPSXRT_WINDOW_WIDTH=7680\nPSXRT_SETTINGS_INI=") && has(root, "play\\launch.txt", "settings.ini\nPSXRT_HINT=1\n"));
    /* what the in-game Graphics menu writes (stubs/psxrt_io.c psxrt_ini_valid) must be accepted here without a warning */
    tree(root, "window_width = 1440\nrender_scale = 3\nscale_mode = integer\nfilter = nearest\nscanlines = on\nwindow_mode = borderless\nhint_shown = 1\n");
    CHECK(prepare(root, &o));
    CHECK(has(root, "play\\game.toml", "window_width = 1440\nsupersampling = 3\nantialiasing = false"));
    CHECK(has(root, "play\\launch.txt", "PSXRT_SCALE_MODE=integer\nPSX_SCANLINES=1\nPSXRT_FULLSCREEN=1\nPSXRT_WINDOW_WIDTH=1440\n"));
    CHECK(!has(root, "play\\launch.txt", "PSXRT_HINT") && !has(root, "play\\settings.warn", "settings.ini"));
    tree(root, "scanlines = maybe\nwindow_mode = full\n");
    CHECK(prepare(root, &o) && has(root, "play\\settings.warn", "scanlines = maybe is not valid; using off\n") && has(root, "play\\settings.warn", "window_mode = full is not valid; using windowed\n"));
    /* UTF-16 / BOM / ANSI settings.ini, and a command line over the Win32 limit */
    tree(root, "x=1\n"); tree16(root, "window_width = 1440\nenv.T = caf\xC3\xA9\n", 0);
    CHECK(prepare(root, &o) && has(root, "play\\game.toml", "window_width = 1440\n") && has(root, "play\\launch.txt", "T=caf\xC3\xA9\n"));
    tree(root, "x=1\n"); tree16(root, "window_width = 1280\n", 1);
    CHECK(prepare(root, &o) && has(root, "play\\game.toml", "window_width = 1280\n") && !has(root, "play\\settings.warn", "UTF-16"));
    tree(root, "\xEF\xBB\xBF" "window_width = 1100\n");
    CHECK(prepare(root, &o) && has(root, "play\\game.toml", "window_width = 1100\n"));
    tree(root, "env.T = caf\xE9\n");
    CHECK(prepare(root, &o) && has(root, "play\\settings.warn", "read as ANSI") && has(root, "play\\launch.txt", "T=caf"));
    tree(root, "x=1\n"); raw(root, "\xFF\xFEw\0i", 5);   /* odd length: undecodable */
    CHECK(prepare(root, &o) && has(root, "play\\settings.warn", "UTF-16 but cannot be decoded"));
    { Sb big = { 0 }; sb_s(&big, "extra_args ="); for (int i = 0; i < 20000; i++) sb_s(&big, " ab"); sb_s(&big, "\n"); tree(root, big.p); }
    CHECK(!prepare(root, &o) && strstr(errmsg, "too long") && has(root, "play\\prepare.err", "too long"));
    { HANDLE a = take_lock(root), b = take_lock(root), c = take_lock(fmt("%sx", root)); char *up = _strdup(root); for (char *k = up; *k; k++) if (*k >= 'a' && *k <= 'z') *k -= 32;
      HANDLE d = take_lock(up);
      CHECK(a && !b && c && !d); }   /* same folder (any case) is taken, another folder is free */
    fprintf(stderr, tfail ? "selftest: %d FAILED\n" : "selftest: all passed\n", tfail);
    return tfail ? 1 : 0;
}

/* ---------- launcher ---------- */

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cl, int sw) {
    int argc; wchar_t **av = CommandLineToArgvW(GetCommandLineW(), &argc);
    int self = 0, only = 0; char *root = NULL;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(av[i], L"--selftest")) self = 1;
        else if (!wcscmp(av[i], L"--prepare")) only = 1;
        else if (!wcscmp(av[i], L"--quiet")) quiet = 1;
        else if (!wcscmp(av[i], L"--root") && i + 1 < argc) root = U(av[++i]);
    }
    if ((self || only || quiet) && !GetStdHandle(STD_ERROR_HANDLE) && AttachConsole(ATTACH_PARENT_PROCESS)) { freopen("CONOUT$", "w", stdout); freopen("CONOUT$", "w", stderr); }   /* redirected stderr (pipe, file) is kept */
    if (self) return selftest();
    if (!root) {
        for (DWORD cap = MAX_PATH; cap <= 65536; cap *= 2) {
            wchar_t *d = malloc(cap * 2); DWORD n = GetModuleFileNameW(NULL, d, cap);
            if (n && n < cap) { *wcsrchr(d, L'\\') = 0; root = U(d); break; }
        }
        if (!root) die("Cannot find the game folder: the path to PLAYTHISGAME.exe is too long. Move the folder somewhere shorter.");
    }
    { char *fr = full_path(root); if (!fr) die("The game folder path is not usable (too long?)."); root = fr; }
    if (!only && !take_lock(root)) {   /* nothing below runs: the first instance keeps its files and log */
        if (!quiet) MessageBoxW(NULL, L"Star Ocean is already running from this folder.", L"Star Ocean", MB_OK | MB_ICONINFORMATION);
        fprintf(stderr, "PLAY: already running from this folder\n"); return 3;
    }
    SetCurrentDirectoryW(W(root));   /* a relative settings.ini bios path resolves against the game folder */
    DeleteFileW(W(fmt("%s\\play\\launch.txt", root)));
    Prep o;
    if (!prepare(root, &o)) { if (only) { fprintf(stderr, "PLAY: %s\n", errmsg); return 1; } die(errmsg); }
    if (only) return 0;
    for (int i = 0; i < o.nenv; i++) { char *eq = strchr(o.env[i], '='); *eq = 0; SetEnvironmentVariableW(W(o.env[i]), W(eq + 1)); }
    wchar_t *cmd = W(fmt("\"%s\" %s", o.exe, o.args));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };   /* game output -> play\last.log / last.err */
    HANDLE out = CreateFileW(W(fmt("%s\\last.log", o.run)), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    HANDLE err = CreateFileW(W(fmt("%s\\last.err", o.run)), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi; DWORD rc = 1;
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = out; si.hStdError = err; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessW(W(o.exe), cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, W(o.run), &si, &pi)) die("Cannot start the game exe. Run INSTALL.bat first.");
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &rc);
    return (int)rc;
}
