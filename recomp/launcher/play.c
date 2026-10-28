/* PLAYTHISGAME.exe: the Steam-friendly launcher, with no PowerShell dependency. "Add a Non-Steam Game" lists only .exe files, and Steam
 * tracks the process it starts, so this waits for the game as a direct child.
 * prepare() is a native port of play.ps1 (kept as the PLAY.bat fallback and as the equivalence reference): it reads settings.ini,
 * writes play\game.toml, play\settings.warn, play\launch.txt, keybinds.ini and config.ini, and fills Prep for the launch.
 * Flags: --play (no window: start the game now; what Steam should use), --prepare (write the files, do not start the game), --selftest
 * (assert-based), --root <dir> (default: the exe's dir). With no flag a small Win32 window shows disc/BIOS/exe status, a Play button and
 * Graphics / Controls / Gameplay pages that edit settings.ini (the in-game F10 keys plus key.* and a few more); settings.ini
 * skip_launcher = 1 makes a double-click behave like --play.
 * Intentional differences from play.ps1: env lines come in settings.ini order (PS hashtable order is unspecified); audio.buffer_ms
 * and pad.deadzone out of range or non-numeric now fall back with a settings.warn note (PS aborted or passed them through).
 * settings.ini may be UTF-8 (BOM or not), UTF-16 (either byte order) or ANSI; it is read as UTF-8 and a note goes to settings.warn when
 * it was converted or could not be decoded. A named mutex (hash of the game folder path) lets one launcher run per folder; a second one
 * exits with code 3 and touches nothing, so a double Steam launch cannot break the running game. --quiet skips message boxes (tests).
 * All strings are UTF-8; they are widened at the Win32 boundary. Build: recomp\launcher\build.ps1 is the standalone way; the
 * installer compiles play.c with cl directly. */
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

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
static int ok_01(const char *v) { return !strcmp(v, "0") || !strcmp(v, "1"); }
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
    put("env.PSXRT_CONFIRM_QUICKLOAD", pick("confirm_quickload", "1", ok_01));   /* F9 / Select+L1 load asks a second press within 3 s; 0 = off */
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

/* last.log / last.err -> last.1.log / last.1.err (the older previous run is replaced), so a crash is still readable after a restart */
static void rotate_logs(const char *run) {
    static const char *n[2][2] = { { "last.log", "last.1.log" }, { "last.err", "last.1.err" } };
    for (int i = 0; i < 2; i++) MoveFileExW(W(fmt("%s\\%s", run, n[i][0])), W(fmt("%s\\%s", run, n[i][1])), MOVEFILE_REPLACE_EXISTING);
}

/* ---------- launcher settings: the editable keys, and the safe settings.ini writer ---------- */
typedef struct { const char *key, *label, *opts, *def; int (*ok)(const char *); int page; } Field;   /* opts NULL = edit box, else "a|b|c" combo */
static int ok_key(const char *v) { size_t n = strlen(v); if (!n || n > 60) return 0; for (; *v; v++) if ((unsigned char)*v < 32 || (unsigned char)*v > 126) return 0; return 1; }
static const char *PAGES[3] = { "Graphics", "Controls", "Gameplay" };
static const Field FIELDS[] = {
    { "window_mode", "Window mode", "windowed|borderless", "windowed", ok_wmode, 0 },
    { "window_width", "Window width (640-7680)", NULL, "960", ok_width, 0 },
    { "render_scale", "Internal resolution (x)", "1|2|3|4|5|6|7|8", "1", ok_scale, 0 },
    { "scale_mode", "Scale mode", "fit|integer|stretch", "fit", ok_mode, 0 },
    { "filter", "Filter", "linear|nearest", "linear", ok_filter, 0 },
    { "scanlines", "Scanlines", "off|on", "off", ok_onoff, 0 },
    { "key.up", "Up", NULL, "W, Up", ok_key, 1 }, { "key.down", "Down", NULL, "S, Down", ok_key, 1 },
    { "key.left", "Left", NULL, "A, Left", ok_key, 1 }, { "key.right", "Right", NULL, "D, Right", ok_key, 1 },
    { "key.cross", "Cross", NULL, "Space, 1", ok_key, 1 }, { "key.circle", "Circle", NULL, "Q, 2", ok_key, 1 },
    { "key.square", "Square", NULL, "E, 3", ok_key, 1 }, { "key.triangle", "Triangle", NULL, "R, 4", ok_key, 1 },
    { "key.l1", "L1", NULL, "Left Shift, Z", ok_key, 1 }, { "key.r1", "R1", NULL, "F, X", ok_key, 1 },
    { "key.l2", "L2", NULL, "Tab, C", ok_key, 1 }, { "key.r2", "R2", NULL, "G, V", ok_key, 1 },
    { "key.l3", "L3", NULL, "T", ok_key, 1 }, { "key.r3", "R3", NULL, "Y", ok_key, 1 },
    { "key.start", "Start", NULL, "Escape, Return", ok_key, 1 }, { "key.select", "Select", NULL, "Backspace, Right Shift", ok_key, 1 },
    { "pad.deadzone", "Stick dead zone (0-32767)", NULL, "9000", ok_dz, 1 },
    { "confirm_quickload", "Confirm quick-load (1 = ask)", "1|0", "1", ok_01, 2 },
    { "audio.buffer_ms", "Audio buffer ms (30-500)", NULL, "180", ok_audio, 2 },
    { "skip_launcher", "Skip this window (1 = on)", "0|1", "0", ok_01, 2 },
};
#define NFIELDS ((int)(sizeof FIELDS / sizeof FIELDS[0]))
static const Field *field_of(const char *k) { for (int i = 0; i < NFIELDS; i++) if (!_stricmp(FIELDS[i].key, k)) return &FIELDS[i]; return NULL; }

/* Same approach as psxrt_ini_set (stubs/psxrt_io.c): the last matching line keeps its text through '=' and gets the new value; an absent
 * key is appended; comments, other keys and line ends stay. */
static char *ini_set_text(const char *t, const char *key, const char *val) {
    size_t kl = strlen(key); const char *hit_end = NULL, *hit_eol = NULL, *p = t;
    while (*p) {
        const char *e = p; while (*e && *e != '\n' && *e != '\r') e++;
        const char *c = p; while (c < e && (*c == ' ' || *c == '\t')) c++;
        if ((size_t)(e - c) > kl && !_strnicmp(c, key, kl)) {
            const char *q = c + kl; while (q < e && (*q == ' ' || *q == '\t')) q++;
            if (q < e && *q == '=') { hit_end = q + 1; hit_eol = e; }
        }
        p = e; if (*p == '\r' && p[1] == '\n') p++; if (*p) p++;
    }
    Sb b = { 0 };
    if (hit_end) { sb_add(&b, t, hit_end - t); sb_f(&b, " %s", val); sb_s(&b, hit_eol); return b.p; }
    const char *eol = strstr(t, "\r\n") ? "\r\n" : "\n"; size_t tl = strlen(t);
    sb_add(&b, t, tl); if (tl && t[tl - 1] != '\n' && t[tl - 1] != '\r') sb_s(&b, eol);
    sb_f(&b, "%s = %s%s", key, val, eol); return b.p;
}
/* 1 saved, 0 write failed, -2 UTF-16 (left untouched), -3 unknown key or invalid value. Temp file + flush + replace; BOM and ANSI bytes kept. */
static int ini_write(const char *root, const char *key, const char *val) {
    const Field *f = field_of(key); if (!f || !f->ok(val)) return -3;
    char *path = fmt("%s\\settings.ini", root), *tmp = fmt("%s\\settings.ini.tmp", root);
    if (!exists(path)) CopyFileW(W(fmt("%s\\settings.default.ini", root)), W(path), TRUE);
    HANDLE h = CreateFileW(W(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 0, got = 0; char *b = NULL;
    if (h != INVALID_HANDLE_VALUE) { n = GetFileSize(h, NULL); b = malloc(n + 1); if (!ReadFile(h, b, n, &got, NULL)) got = 0; CloseHandle(h); n = got; b[n] = 0; }
    else b = _strdup("");
    unsigned char *u = (unsigned char *)b;
    if (n >= 2 && ((u[0] == 0xFF && u[1] == 0xFE) || (u[0] == 0xFE && u[1] == 0xFF))) return -2;
    if (strlen(b) != n) return -2;   /* NUL bytes: UTF-16 without a BOM */
    int bom = n >= 3 && !memcmp(b, "\xEF\xBB\xBF", 3); char *out = ini_set_text(b + (bom ? 3 : 0), key, val);
    HANDLE w = CreateFileW(W(tmp), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (w == INVALID_HANDLE_VALUE) return 0;
    DWORD m = (DWORD)strlen(out), g2 = 0, g1 = 0; int ok = 1;
    if (bom) ok = WriteFile(w, "\xEF\xBB\xBF", 3, &g1, NULL) && g1 == 3;
    ok = ok && WriteFile(w, out, m, &g2, NULL) && g2 == m && FlushFileBuffers(w);
    CloseHandle(w);
    if (!ok || !MoveFileExW(W(tmp), W(path), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(W(tmp)); return 0; }
    return 1;
}
static void ini_load(const char *root) {   /* settings.ini -> get(); missing file = empty (defaults) */
    size_t n; char *t = read_file(fmt("%s\\settings.ini", root), &n); Sb scratch = { 0 };
    parse_ini(t ? ini_decode(t, n, &scratch) : "");
}

/* ---------- status rows: what the Play button needs ---------- */
static void status_rows(const char *root, int ok[4], char *txt[4]) {
    for (int n = 1; n <= 2; n++) {
        char *c = fmt("%s\\roms\\disc%d\\%s.cue", root, n, fmt(DISC, n)); ok[n - 1] = exists(c);
        txt[n - 1] = fmt(ok[n - 1] ? "Disc %d: found" : "Disc %d: missing (roms\\disc%d, run INSTALL.bat)", n, n);
    }
    const char *b = get("bios"); char *own = fmt("%s\\roms\\scph5501.bin", root), *ob = fmt("%s\\tools\\ref\\psxrecomp\\bios\\openbios.bin", root);
    if (!*b || !_stricmp(b, "auto")) {
        int o = exists(own) && exists(fmt("%s\\recomp\\bios\\_gen\\generated\\SCPH5501_full.c", root));
        ok[2] = o || exists(ob); txt[2] = fmt(!ok[2] ? "BIOS: missing (run INSTALL.bat)" : o ? "BIOS: your SCPH-5501 dump" : "BIOS: OpenBIOS (bundled)");
    } else if (!_stricmp(b, "openbios")) { ok[2] = exists(ob); txt[2] = fmt(ok[2] ? "BIOS: OpenBIOS (bundled)" : "BIOS: OpenBIOS missing (run INSTALL.bat)"); }
    else { ok[2] = exists(b); txt[2] = fmt(ok[2] ? "BIOS: %s" : "BIOS: %s not found", b); }
    ok[3] = exists(fmt("%s\\recomp\\rtfull\\_build_final\\psxrecomp-game.exe", root));
    txt[3] = fmt(ok[3] ? "Game: built" : "Game: not built (run INSTALL.bat)");
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
    tree(root, "confirm_quickload = 0\n");
    CHECK(prepare(root, &o) && has(root, "play\\launch.txt", "PSXRT_CONFIRM_QUICKLOAD=0\n"));
    tree(root, "confirm_quickload = yes\n");
    CHECK(prepare(root, &o) && has(root, "play\\launch.txt", "PSXRT_CONFIRM_QUICKLOAD=1\n") && has(root, "play\\settings.warn", "confirm_quickload = yes is not valid; using 1\n"));
    tree(root, "x = 1\n");
    CHECK(prepare(root, &o) && has(root, "play\\launch.txt", "PSXRT_CONFIRM_QUICKLOAD=1\n"));   /* default: confirm */
    { /* non-ASCII game folder: the settings.ini path reaches the game as UTF-8 text in the (wide) environment */
        char *r2 = fmt("%s_\xC3\xA9" "\xE4\xB8\xAD", root); tree(r2, "x = 1\n");
        CHECK(prepare(r2, &o) && has(r2, "play\\launch.txt", "PSXRT_SETTINGS_INI=") && has(r2, "play\\launch.txt", "\xC3\xA9" "\xE4\xB8\xAD\\settings.ini\n"));
    }
    { /* log rotation keeps exactly one previous run */
        mk(root, "play\\last.log", "run A log"); mk(root, "play\\last.err", "run A err");
        rotate_logs(fmt("%s\\play", root));
        CHECK(has(root, "play\\last.1.log", "run A log") && has(root, "play\\last.1.err", "run A err") && !exists(fmt("%s\\play\\last.log", root)));
        mk(root, "play\\last.log", "run B log"); mk(root, "play\\last.err", "run B err");
        rotate_logs(fmt("%s\\play", root));
        CHECK(has(root, "play\\last.1.log", "run B log") && !has(root, "play\\last.1.log", "run A") && has(root, "play\\last.1.err", "run B err"));
    }
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
    { /* launcher: status rows and the settings.ini writer */
        int sok[4]; char *stx[4]; unsigned char bb[3]; DWORD g; HANDLE hh;
        tree(root, "; keep\r\nbios = auto\r\nFilter = linear\r\nunknown.key = 7\r\n");
        ini_load(root); status_rows(root, sok, stx);
        for (int i = 0; i < 4; i++) fprintf(stderr, "status %c %s\n", sok[i] ? '+' : '-', stx[i]);
        CHECK(sok[0] && sok[1] && sok[2] && sok[3]);
        CHECK(ini_write(root, "filter", "nearest") == 1 && ini_write(root, "scanlines", "on") == 1 && ini_write(root, "key.cross", "K, J") == 1);
        CHECK(has(root, "settings.ini", "; keep\r\nbios = auto\r\nFilter = nearest\r\nunknown.key = 7\r\nscanlines = on\r\nkey.cross = K, J\r\n"));
        ini_load(root); CHECK(!strcmp(get("filter"), "nearest") && !strcmp(get("key.cross"), "K, J") && !strcmp(get("unknown.key"), "7"));
        CHECK(ini_write(root, "filter", "bogus") == -3 && ini_write(root, "window_width", "639") == -3 && ini_write(root, "nokey", "1") == -3 && ini_write(root, "key.up", "a\nb") == -3);
        CHECK(!exists(fmt("%s\\settings.ini.tmp", root)));
        tree(root, "\xEF\xBB\xBF" "a = 1\n"); CHECK(ini_write(root, "skip_launcher", "1") == 1 && has(root, "settings.ini", "a = 1\nskip_launcher = 1\n"));
        hh = CreateFileW(W(fmt("%s\\settings.ini", root)), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); memset(bb, 0, 3); ReadFile(hh, bb, 3, &g, NULL); CloseHandle(hh);
        CHECK(bb[0] == 0xEF && bb[1] == 0xBB && bb[2] == 0xBF);   /* BOM kept */
        tree(root, "x=1\n"); tree16(root, "filter = linear\n", 0);
        CHECK(ini_write(root, "filter", "nearest") == -2);   /* UTF-16 refused, file untouched */
        hh = CreateFileW(W(fmt("%s\\settings.ini", root)), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); memset(bb, 0, 3); ReadFile(hh, bb, 2, &g, NULL); CloseHandle(hh);
        CHECK(bb[0] == 0xFF && bb[1] == 0xFE);
        for (int i = 0; i < NFIELDS; i++) CHECK(FIELDS[i].ok(FIELDS[i].def));   /* every default passes its own validator */
    }
    fprintf(stderr, tfail ? "selftest: %d FAILED\n" : "selftest: all passed\n", tfail);
    return tfail ? 1 : 0;
}

/* ---------- launcher ---------- */
static HWND g_hide;   /* the launcher window, hidden while the game runs */

/* prepare + start the game and wait for it. 0 = could not start (message in errmsg); 1 = ran, exit code in *rc. */
static int launch_game(const char *root, int *rc) {
    DeleteFileW(W(fmt("%s\\play\\launch.txt", root)));
    Prep o;
    if (!prepare(root, &o)) return 0;
    for (int i = 0; i < o.nenv; i++) { char *eq = strchr(o.env[i], '='); *eq = 0; SetEnvironmentVariableW(W(o.env[i]), W(eq + 1)); }
    wchar_t *cmd = W(fmt("\"%s\" %s", o.exe, o.args));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };   /* game output -> play\last.log / last.err */
    rotate_logs(o.run);
    HANDLE out = CreateFileW(W(fmt("%s\\last.log", o.run)), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    HANDLE err = CreateFileW(W(fmt("%s\\last.err", o.run)), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi; DWORD c = 1;
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = out; si.hStdError = err; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (g_hide) ShowWindow(g_hide, SW_HIDE);
    if (!CreateProcessW(W(o.exe), cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, W(o.run), &si, &pi)) {
        if (g_hide) ShowWindow(g_hide, SW_SHOW);
        snprintf(errmsg, sizeof errmsg, "Cannot start the game exe. Run INSTALL.bat first."); return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &c); *rc = (int)c; return 1;
}

/* ---------- the window ---------- */
#define ID_PLAY 900
#define ID_TAB 901
#define ID_F0 1000
static struct { HWND tab, play, msg, st[4], lbl[64], ctl[64]; const char *root; int rc; } G;

static void ui_set(int i, const char *v) {   /* show v in field i's control */
    const Field *f = &FIELDS[i];
    if (!f->opts) { SetWindowTextW(G.ctl[i], W(v)); return; }
    int idx = 0, n = 0; char *o = _strdup(f->opts);
    for (char *t = strtok(o, "|"); t; t = strtok(NULL, "|"), n++) if (!_stricmp(t, v)) idx = n;
    SendMessageW(G.ctl[i], CB_SETCURSEL, idx, 0);
}
static void ui_status(void) {
    int ok[4]; char *tx[4]; ini_load(G.root); status_rows(G.root, ok, tx);
    for (int i = 0; i < 4; i++) SetWindowTextW(G.st[i], W(fmt("%s  %s", ok[i] ? "\xE2\x9C\x94" : "\xE2\x9C\x96", tx[i])));
    EnableWindow(G.play, ok[0] && ok[1] && ok[2] && ok[3]);
}
static void ui_reload(void) {
    ui_status();
    for (int i = 0; i < NFIELDS; i++) { const char *v = get(FIELDS[i].key); char *l = lower(v); ui_set(i, *l && FIELDS[i].ok(l) ? l : FIELDS[i].def); }
}
static void ui_page(void) {
    int p = (int)SendMessageW(G.tab, TCM_GETCURSEL, 0, 0);
    for (int i = 0; i < NFIELDS; i++) { int s = FIELDS[i].page == p ? SW_SHOW : SW_HIDE; ShowWindow(G.lbl[i], s); ShowWindow(G.ctl[i], s); }
}
static void ui_commit(int i) {   /* field i changed: validate and save; a bad edit snaps back to the saved value */
    const Field *f = &FIELDS[i]; wchar_t w[128] = L""; char v[256];
    if (f->opts) { int s = (int)SendMessageW(G.ctl[i], CB_GETCURSEL, 0, 0); if (s >= 0) SendMessageW(G.ctl[i], CB_GETLBTEXT, s, (LPARAM)w); }
    else GetWindowTextW(G.ctl[i], w, 128);
    snprintf(v, sizeof v, "%s", U(w)); { char *t = v; while (isws(*t)) t++; memmove(v, t, strlen(t) + 1); for (size_t n = strlen(v); n && isws(v[n - 1]); ) v[--n] = 0; }
    ini_load(G.root);
    if (!strcmp(v, get(f->key)) || (!*get(f->key) && !strcmp(v, f->def))) return;   /* unchanged: leave the file alone */
    int r = ini_write(G.root, f->key, v);
    SetWindowTextW(G.msg, W(r == 1 ? fmt("Saved %s = %s", f->key, v) : r == -2 ? "Not saved: settings.ini is UTF-16. Save it as UTF-8." :
        r == -3 ? fmt("Not saved: %s is not a valid value for %s", v, f->key) : "Not saved: cannot write settings.ini"));
    if (r != 1) { ini_load(G.root); const char *o = get(f->key); char *l = lower(o); ui_set(i, *l && f->ok(l) ? l : f->def); }
}
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        NONCLIENTMETRICSW nm = { sizeof nm }; SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof nm, &nm, 0);
        HFONT font = CreateFontIndirectW(&nm.lfMessageFont), big = (nm.lfMessageFont.lfHeight *= 2, CreateFontIndirectW(&nm.lfMessageFont));
        HINSTANCE hi = GetModuleHandleW(NULL);
        for (int i = 0; i < 4; i++) { G.st[i] = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 16, 10 + i * 22, 470, 20, h, NULL, hi, NULL); SendMessageW(G.st[i], WM_SETFONT, (WPARAM)font, 0); }
        G.play = CreateWindowExW(0, L"BUTTON", L"Play", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 16, 104, 470, 56, h, (HMENU)ID_PLAY, hi, NULL);
        SendMessageW(G.play, WM_SETFONT, (WPARAM)big, 0);
        G.tab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS, 16, 172, 470, 290, h, (HMENU)ID_TAB, hi, NULL);
        SendMessageW(G.tab, WM_SETFONT, (WPARAM)font, 0);
        for (int p = 0; p < 3; p++) { TCITEMW it = { TCIF_TEXT }; it.pszText = W(PAGES[p]); SendMessageW(G.tab, TCM_INSERTITEMW, p, (LPARAM)&it); }
        int cnt[3] = { 0 };
        for (int i = 0; i < NFIELDS; i++) {
            const Field *f = &FIELDS[i]; int n = cnt[f->page]++, two = f->page == 1 && n < 16;
            int col = two ? n / 8 : 0, row = two ? n % 8 : (f->page == 1 ? 8 : n), lw = two ? 70 : 220, ew = two ? 150 : 150;
            int x = 32 + col * 230, y = 208 + row * 28;
            G.lbl[i] = CreateWindowExW(0, L"STATIC", W(f->label), WS_CHILD, x, y + 3, lw, 20, h, NULL, hi, NULL);
            if (f->opts) {
                G.ctl[i] = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, x + lw, y, ew, 200, h, (HMENU)(INT_PTR)(ID_F0 + i), hi, NULL);
                char *o = _strdup(f->opts); for (char *t = strtok(o, "|"); t; t = strtok(NULL, "|")) SendMessageW(G.ctl[i], CB_ADDSTRING, 0, (LPARAM)W(t));
            } else G.ctl[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, x + lw, y, ew, 22, h, (HMENU)(INT_PTR)(ID_F0 + i), hi, NULL);
            SendMessageW(G.lbl[i], WM_SETFONT, (WPARAM)font, 0); SendMessageW(G.ctl[i], WM_SETFONT, (WPARAM)font, 0);
        }
        G.msg = CreateWindowExW(0, L"STATIC", L"Changes are saved to settings.ini as you make them.", WS_CHILD | WS_VISIBLE, 16, 470, 470, 20, h, NULL, hi, NULL);
        SendMessageW(G.msg, WM_SETFONT, (WPARAM)font, 0);
        ui_reload(); ui_page(); return 0; }
    case WM_NOTIFY:
        if (((NMHDR *)lp)->idFrom == ID_TAB && ((NMHDR *)lp)->code == TCN_SELCHANGE) ui_page();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), nc = HIWORD(wp);
        if (id == ID_PLAY) {
            g_hide = h; int rc = 0;
            if (launch_game(G.root, &rc)) { G.rc = rc; DestroyWindow(h); }
            else { MessageBoxW(h, W(errmsg), L"Star Ocean: not ready", MB_OK | MB_ICONERROR); ui_status(); }
        } else if (id >= ID_F0 && id < ID_F0 + NFIELDS && (nc == CBN_SELCHANGE || nc == EN_KILLFOCUS)) ui_commit(id - ID_F0);
        return 0; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
static int run_gui(const char *root) {
    INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_TAB_CLASSES }; InitCommonControlsEx(&ic);
    HINSTANCE hi = GetModuleHandleW(NULL); G.root = root;
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = wndproc; wc.hInstance = hi; wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SO2Launcher"; wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1)); RegisterClassW(&wc);
    RECT r = { 0, 0, 502, 500 }; AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND h = CreateWindowW(L"SO2Launcher", L"Star Ocean: The Second Story", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    if (!h) return 1;
    MSG msg; while (GetMessageW(&msg, NULL, 0, 0) > 0) { if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    return G.rc;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cl, int sw) {
    int argc; wchar_t **av = CommandLineToArgvW(GetCommandLineW(), &argc);
    int self = 0, only = 0, play = 0; char *root = NULL;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(av[i], L"--selftest")) self = 1;
        else if (!wcscmp(av[i], L"--prepare")) only = 1;
        else if (!wcscmp(av[i], L"--play")) play = 1;
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
    if (!only && !play && !quiet) {   /* window, unless settings.ini says skip_launcher = 1 */
        ini_load(root);
        if (strcmp(get("skip_launcher"), "1")) return run_gui(root);
    }
    if (only) { DeleteFileW(W(fmt("%s\\play\\launch.txt", root))); Prep o; if (!prepare(root, &o)) { fprintf(stderr, "PLAY: %s\n", errmsg); return 1; } return 0; }
    int rc = 0;
    if (!launch_game(root, &rc)) die(errmsg);
    return rc;
}
