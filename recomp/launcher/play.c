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

/* ---------- launcher option tables: plain-English labels <-> settings.ini values (no window needed, so --selftest covers them) ---------- */
typedef struct { const char *val, *label; } Opt;
typedef struct { const char *key, *label; const Opt *o; int n; } Dd;
static const Opt O_WMODE[] = { { "windowed", "Windowed" }, { "borderless", "Borderless fullscreen" } };
static const Opt O_SIZE[] = { { "960", "720p  960 x 720" }, { "1280", "960p  1280 x 960" }, { "1440", "1080p  1440 x 1080" }, { "1920", "1440p  1920 x 1440" }, { "2880", "2160p  2880 x 2160" } };
static const Opt O_SCALE[] = { { "1", "Native PS1 - 1x (320 x 240)" }, { "2", "2x (640 x 480)" }, { "3", "3x (960 x 720)" }, { "4", "4x (1280 x 960)" },
    { "5", "5x (1600 x 1200)" }, { "6", "6x (1920 x 1440)" }, { "7", "7x (2240 x 1680)" }, { "8", "8x (2560 x 1920)" } };
static const Opt O_FIT[] = { { "fit", "Fit (keep 4:3)" }, { "integer", "Pixel perfect (integer)" }, { "stretch", "Stretch to window" } };
static const Opt O_FILT[] = { { "linear", "Smooth" }, { "nearest", "Sharp pixels" } };
enum { D_WMODE, D_SIZE, D_SCALE, D_FIT, D_FILT };
static const Dd DDS[] = { { "window_mode", "Window mode", O_WMODE, 2 }, { "window_width", "Window size", O_SIZE, 5 }, { "render_scale", "Render resolution", O_SCALE, 8 },
    { "scale_mode", "Picture fit", O_FIT, 3 }, { "filter", "Smoothing", O_FILT, 2 } };
static int opt_find(const Opt *o, int n, const char *v) { for (int i = 0; i < n; i++) if (!_stricmp(o[i].val, v)) return i; return -1; }
static int dd_cur(int d) {   /* index of the current settings.ini value (needs ini_load); bad/missing -> the default; a window width between sizes -> the nearest */
    const Dd *x = &DDS[d]; const char *v = get(x->key);
    if (d == D_SIZE) { int w = ok_width(v) ? atoi(v) : 960, b = 0, bd = 1 << 30; for (int i = 0; i < x->n; i++) { int e = abs(atoi(x->o[i].val) - w); if (e < bd) { bd = e; b = i; } } return b; }
    int i = opt_find(x->o, x->n, v); if (i < 0) i = opt_find(x->o, x->n, field_of(x->key)->def); return i < 0 ? 0 : i;
}
static int best_size(int cx, int cy) { int b = 0; for (int i = 0; i < 5; i++) { int w = atoi(O_SIZE[i].val); if (w <= cx && w * 3 / 4 <= cy - 80) b = i; } return b; }   /* largest window that fits, leaving room for the title bar and taskbar */

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
    { /* launcher option tables: every label value is accepted by the validator and round-trips through settings.ini */
        tree(root, "x = 1\n");
        for (int d = 0; d < 5; d++) {
            CHECK(dd_cur(d) == opt_find(DDS[d].o, DDS[d].n, field_of(DDS[d].key)->def));   /* empty ini -> default */
            for (int i = 0; i < DDS[d].n; i++) {
                CHECK(field_of(DDS[d].key)->ok(DDS[d].o[i].val) && ini_write(root, DDS[d].key, DDS[d].o[i].val) == 1);
                ini_load(root); CHECK(dd_cur(d) == i);
            }
        }
        CHECK(ini_write(root, "window_width", "1100") == 1); ini_load(root); CHECK(dd_cur(D_SIZE) == 0);   /* between sizes: nearest (1100 is 140 from 960, 180 from 1280) */
        CHECK(best_size(2560, 1440) == 2 && best_size(3840, 2160) == 3 && best_size(1920, 1080) == 1 && best_size(800, 600) == 0);
    }
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

/* ---------- the window: custom-painted with GDI+ (flat API, declared here so no C++ headers are needed), double-buffered, one hit-test list ---------- */
#define GP __stdcall
typedef struct { UINT32 v; void *cb; BOOL st, sc; } GsIn;
typedef struct { float x, y, w, h; } RectF;
int GP GdiplusStartup(ULONG_PTR *, const GsIn *, void *);
int GP GdipCreateFromHDC(HDC, void **); int GP GdipDeleteGraphics(void *);
int GP GdipSetSmoothingMode(void *, int); int GP GdipSetTextRenderingHint(void *, int); int GP GdipSetInterpolationMode(void *, int);
int GP GdipScaleWorldTransform(void *, float, float, int);
int GP GdipCreateSolidFill(UINT, void **); int GP GdipDeleteBrush(void *);
int GP GdipFillRectangle(void *, void *, float, float, float, float); int GP GdipFillEllipse(void *, void *, float, float, float, float);
int GP GdipCreatePath(int, void **); int GP GdipDeletePath(void *);
int GP GdipAddPathArc(void *, float, float, float, float, float, float); int GP GdipClosePathFigure(void *);
int GP GdipAddPathEllipse(void *, float, float, float, float); int GP GdipAddPathLine(void *, float, float, float, float);
int GP GdipFillPath(void *, void *, void *); int GP GdipDrawPath(void *, void *, void *);
int GP GdipCreatePen1(UINT, float, int, void **); int GP GdipDeletePen(void *);
int GP GdipDrawLine(void *, void *, float, float, float, float); int GP GdipDrawArc(void *, void *, float, float, float, float, float, float);
int GP GdipCreateLineBrushFromRect(const RectF *, UINT, UINT, int, int, void **);
int GP GdipCreatePathGradientFromPath(void *, void **); int GP GdipSetPathGradientCenterColor(void *, UINT);
int GP GdipSetPathGradientSurroundColorsWithCount(void *, const UINT *, int *);
int GP GdipCreateFontFamilyFromName(const WCHAR *, void *, void **); int GP GdipGetGenericFontFamilySansSerif(void **); int GP GdipGetGenericFontFamilySerif(void **);
int GP GdipCreateFont(void *, float, int, int, void **);
int GP GdipCreateStringFormat(int, int, void **); int GP GdipStringFormatGetGenericTypographic(void **);
int GP GdipSetStringFormatAlign(void *, int); int GP GdipSetStringFormatLineAlign(void *, int); int GP GdipSetStringFormatFlags(void *, int);
int GP GdipDrawString(void *, const WCHAR *, int, void *, const RectF *, void *, void *);
int GP GdipMeasureString(void *, const WCHAR *, int, void *, const RectF *, void *, RectF *, int *, int *);
int GP GdipCreateBitmapFromFile(const WCHAR *, void **); int GP GdipGetImageWidth(void *, UINT *); int GP GdipGetImageHeight(void *, UINT *);
int GP GdipDrawImageRectRect(void *, void *, float, float, float, float, float, float, float, float, int, void *, void *, void *);
int GP GdipSetClipRect(void *, float, float, float, float, int); int GP GdipResetClip(void *);
int GP GdipCreateBitmapFromHBITMAP(HBITMAP, HPALETTE, void **); int GP GdipSaveImageToFile(void *, const WCHAR *, const CLSID *, const void *);

#define RGBA(c, a) (((UINT)(a) << 24) | ((UINT)(c) & 0xFFFFFF))
#define C_BG 0xFF0B1020u
#define C_PANEL 0xFF111A36u
#define C_LINE 0xFF2A3560u
#define C_TXT 0xFFE6EAF7u
#define C_MUT 0xFF8D97B8u
#define C_TEAL 0xFF3DD6C6u
#define C_GOLD 0xFFE8B857u
#define C_GREEN 0xFF4CD08Au
#define C_RED 0xFFE0566Bu
enum { K_NAV, K_DD, K_CHK, K_PLAY };
enum { F_BODY, F_SM, F_H1, F_NAV, F_BTN, F_LOGO, F_TAG, F_LAB, F_SUBLOGO, NF };
typedef struct { int kind, id, en; float x, y, w, h; } It;
static struct { HWND h; const char *root; int rc, page, focus, hover, nit, ok[4], track; char *tx[4]; char toast[128]; int toast_err; float s; It it[16]; void *banner, *fnt[NF], *sf, *tf; } G;
static void *gg;   /* the Graphics being painted */
static const char *NAVS[3] = { "Game", "Graphics", "Controls" };

static void refresh(void) { ini_load(G.root); status_rows(G.root, G.ok, G.tx); }
static int all_ready(void) { return G.ok[0] && G.ok[1] && G.ok[2] && G.ok[3]; }
static int fs_mode(void) { return dd_cur(D_WMODE) == 1; }
static void layout(void) {
    It *t = G.it; int n = 0;
    for (int i = 0; i < 3; i++) t[n++] = (It){ K_NAV, i, 1, 12, 290 + i * 56, 216, 48 };
    if (G.page == 0) { t[n++] = (It){ K_PLAY, 0, all_ready(), 280, 510, 680, 62 }; t[n++] = (It){ K_CHK, 1, 1, 280, 440, 680, 30 }; }
    else if (G.page == 1) {
        t[n++] = (It){ K_DD, D_WMODE, 1, 280, 284, 320, 38 }; t[n++] = (It){ K_DD, D_SIZE, !fs_mode(), 280, 368, 320, 38 };
        t[n++] = (It){ K_DD, D_SCALE, 1, 280, 452, 320, 38 }; t[n++] = (It){ K_DD, D_FIT, 1, 640, 284, 320, 38 };
        t[n++] = (It){ K_DD, D_FILT, 1, 640, 368, 320, 38 }; t[n++] = (It){ K_CHK, 0, 1, 640, 452, 320, 38 };
    }
    G.nit = n;
}
static int chk_on(int id) { return id == 0 ? !strcmp(get("scanlines"), "on") : !strcmp(get("skip_launcher"), "1"); }

/* ---- drawing helpers (logical 1000 x 640 units; the world transform scales them to the DPI) ---- */
static void fillr(UINT c, float x, float y, float w, float h) { void *b; GdipCreateSolidFill(c, &b); GdipFillRectangle(gg, b, x, y, w, h); GdipDeleteBrush(b); }
static void fille(UINT c, float x, float y, float w, float h) { void *b; GdipCreateSolidFill(c, &b); GdipFillEllipse(gg, b, x, y, w, h); GdipDeleteBrush(b); }
static void line_(UINT c, float pw, float x1, float y1, float x2, float y2) { void *p; GdipCreatePen1(c, pw, 2, &p); GdipDrawLine(gg, p, x1, y1, x2, y2); GdipDeletePen(p); }
static void arc_(UINT c, float pw, float x, float y, float w, float h, float a, float sw) { void *p; GdipCreatePen1(c, pw, 2, &p); GdipDrawArc(gg, p, x, y, w, h, a, sw); GdipDeletePen(p); }
static void vgrad(float x, float y, float w, float h, UINT c1, UINT c2) { RectF r = { x, y, w, h }; void *b; GdipCreateLineBrushFromRect(&r, c1, c2, 1, 3, &b); GdipFillRectangle(gg, b, x, y, w, h); GdipDeleteBrush(b); }
static void glow(float cx, float cy, float rx, float ry, UINT rgb, int a) {   /* soft radial glow */
    void *p, *b; UINT sur = RGBA(rgb, 0); int one = 1;
    GdipCreatePath(0, &p); GdipAddPathEllipse(p, cx - rx, cy - ry, rx * 2, ry * 2); GdipCreatePathGradientFromPath(p, &b);
    GdipSetPathGradientCenterColor(b, RGBA(rgb, a)); GdipSetPathGradientSurroundColorsWithCount(b, &sur, &one);
    GdipFillPath(gg, b, p); GdipDeleteBrush(b); GdipDeletePath(p);
}
static void *rrpath(float x, float y, float w, float h, float r) {
    void *p; GdipCreatePath(0, &p); float d = r * 2;
    GdipAddPathArc(p, x, y, d, d, 180, 90); GdipAddPathArc(p, x + w - d, y, d, d, 270, 90); GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90); GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p); return p;
}
static void rr(UINT fill, UINT stroke, float sw, float x, float y, float w, float h, float r) {
    void *p = rrpath(x, y, w, h, r);
    if (fill) { void *b; GdipCreateSolidFill(fill, &b); GdipFillPath(gg, b, p); GdipDeleteBrush(b); }
    if (stroke) { void *pen; GdipCreatePen1(stroke, sw, 2, &pen); GdipDrawPath(gg, pen, p); GdipDeletePen(pen); }
    GdipDeletePath(p);
}
static void rrgrad(float x, float y, float w, float h, float r, UINT c1, UINT c2) {   /* vertical gradient in a rounded rectangle */
    RectF rc = { x, y, w, h }; void *b, *p = rrpath(x, y, w, h, r);
    GdipCreateLineBrushFromRect(&rc, c1, c2, 1, 3, &b); GdipFillPath(gg, b, p); GdipDeleteBrush(b); GdipDeletePath(p);
}
static void text(const char *s, float x, float y, float w, float h, int font, UINT c, int ha, int va) {   /* ha/va 0 near, 1 centre, 2 far */
    static wchar_t wb[1024]; MultiByteToWideChar(CP_UTF8, 0, s, -1, wb, 1024);
    RectF r = { x, y, w, h }; void *b; GdipCreateSolidFill(c, &b);
    GdipSetStringFormatAlign(G.sf, ha); GdipSetStringFormatLineAlign(G.sf, va);
    GdipDrawString(gg, wb, -1, G.fnt[font], &r, G.sf, b); GdipDeleteBrush(b);
}
static void spaced(const char *s, float x, float y, int font, UINT c, float sp, int centre) {   /* letter-spaced caps; x is the left edge, or the centre if centre */
    int n = (int)strlen(s); float wd[64], tot = 0; if (n > 63) n = 63;
    for (int i = 0; i < n; i++) { WCHAR ch = (unsigned char)s[i]; RectF l = { 0, 0, 1000, 100 }, o = { 0 }; GdipMeasureString(gg, &ch, 1, G.fnt[font], &l, G.tf, &o, NULL, NULL); wd[i] = o.w; tot += o.w + sp; }
    tot -= sp; if (centre) x -= tot / 2;
    for (int i = 0; i < n; i++) { char one[2] = { s[i], 0 }; text(one, x, y, 60, 40, font, c, 0, 0); x += wd[i] + sp; }
}
static void stars(float x, float y, float w, float h, int n, unsigned seed, float maxsz) {   /* deterministic starfield */
    for (int i = 0; i < n; i++) {
        seed = seed * 1664525u + 1013904223u; float fx = (seed >> 8 & 0xFFFF) / 65535.f;
        seed = seed * 1664525u + 1013904223u; float fy = (seed >> 8 & 0xFFFF) / 65535.f;
        seed = seed * 1664525u + 1013904223u; float fs = (seed >> 8 & 0xFFFF) / 65535.f;
        UINT col = (i % 9 == 0) ? 0xF2D38A : 0xDDE6FF; float sz = 0.7f + fs * fs * maxsz;
        fille(RGBA(col, 70 + (int)(fs * 185)), x + fx * w, y + fy * h, sz, sz);
        if (sz > maxsz * 0.7f) { line_(RGBA(col, 60), 1, x + fx * w - sz * 2.5f, y + fy * h + sz / 2, x + fx * w + sz * 3.5f, y + fy * h + sz / 2); line_(RGBA(col, 60), 1, x + fx * w + sz / 2, y + fy * h - sz * 2.5f, x + fx * w + sz / 2, y + fy * h + sz * 3.5f); }
    }
}
static void star4(float cx, float cy, float R, float r, UINT c) {   /* four-pointed star */
    void *p, *b; GdipCreatePath(0, &p); float q = r * .45f, pt[8][2] = { { 0, -R }, { q, -q }, { R, 0 }, { q, q }, { 0, R }, { -q, q }, { -R, 0 }, { -q, -q } };
    for (int i = 0; i < 8; i++) { int j = (i + 1) % 8; GdipAddPathLine(p, cx + pt[i][0], cy + pt[i][1], cx + pt[j][0], cy + pt[j][1]); }
    GdipClosePathFigure(p); GdipCreateSolidFill(c, &b); GdipFillPath(gg, b, p); GdipDeleteBrush(b); GdipDeletePath(p);
}
static void draw_banner(float x, float y, float w, float h) {
    GdipSetClipRect(gg, x, y, w, h, 0);
    if (G.banner) {   /* the optional user-supplied banner.png, cover-scaled and centred */
        UINT iw, ih; GdipGetImageWidth(G.banner, &iw); GdipGetImageHeight(G.banner, &ih);
        float sc = (w / iw > h / ih) ? w / iw : h / ih, sw = w / sc, sh = h / sc;
        GdipDrawImageRectRect(gg, G.banner, x, y, w, h, (iw - sw) / 2, (ih - sh) / 2, sw, sh, 2, NULL, NULL, NULL);
    } else {
        vgrad(x, y, w, h, 0xFF050818, 0xFF1B2B6B);
        glow(x + w * 0.25f, y + h * 0.45f, 260, 120, 0x6B3FA0, 70); glow(x + w * 0.62f, y + h * 0.9f, 300, 110, 0x2AA9C0, 80);
        stars(x, y, w, h, 150, 20261027u, 2.2f);
        float px = x + w * 0.74f, py = y + h * 1.55f, pr = 230;   /* planet rising from below, with atmosphere glow and a golden ring */
        glow(px, py, pr + 60, pr + 60, 0x3DD6C6, 110);
        { RectF r = { px - pr, py - pr, pr * 2, pr * 2 }; void *b; GdipCreateLineBrushFromRect(&r, 0xFF48C9C0, 0xFF0A1633, 1, 3, &b); GdipFillEllipse(gg, b, px - pr, py - pr, pr * 2, pr * 2); GdipDeleteBrush(b); }
        arc_(RGBA(0xA8FFF4, 200), 2.5f, px - pr, py - pr, pr * 2, pr * 2, 195, 150);
        arc_(RGBA(C_GOLD, 120), 1.5f, px - pr - 70, py - pr - 18, (pr + 70) * 2, (pr + 18) * 2, 205, 130);
        glow(x + w * 0.42f, y + 38, 26, 26, 0xF2D38A, 120); fille(0xFFF6E3AE, x + w * 0.42f - 8, y + 30, 16, 16);   /* a small gold moon */
        line_(RGBA(C_GOLD, 170), 1.5f, x + w * 0.12f, y + 30, x + w * 0.12f + 70, y + 52); line_(RGBA(C_GOLD, 60), 1, x + w * 0.12f - 50, y + 12, x + w * 0.12f, y + 30);
    }
    vgrad(x, y + h - 60, w, 60, RGBA(C_BG, 0), C_BG);
    GdipResetClip(gg);
}

static void render(HDC dc) {
    GdipCreateFromHDC(dc, &gg); GdipSetSmoothingMode(gg, 4); GdipSetTextRenderingHint(gg, 4); GdipSetInterpolationMode(gg, 7); GdipScaleWorldTransform(gg, G.s, G.s, 0);
    layout();
    fillr(C_BG, 0, 0, 1000, 640);
    /* sidebar */
    vgrad(0, 0, 240, 640, 0xFF060A18, 0xFF0A1230); stars(0, 0, 240, 640, 55, 777u, 1.6f);
    line_(0xFF1A2347, 1, 240, 0, 240, 640);
    glow(120, 190, 100, 100, 0x3DD6C6, 40);
    spaced("STAR OCEAN", 120, 36, F_LOGO, C_TXT, 2.5f, 1);
    line_(RGBA(C_GOLD, 140), 1, 40, 82, 104, 82); line_(RGBA(C_GOLD, 140), 1, 136, 82, 200, 82); star4(120, 82, 6, 5, C_GOLD);
    spaced("THE SECOND STORY", 120, 94, F_SUBLOGO, C_GOLD, 2.2f, 1);
    arc_(RGBA(C_TEAL, 150), 2, 80, 140, 80, 80, 0, 360); arc_(RGBA(C_GOLD, 120), 1.2f, 66, 150, 108, 60, 160, 220); star4(120, 180, 22, 10, 0xFFF6E3AE);
    for (int i = 0; i < 3; i++) {
        const It *t = &G.it[i]; int sel = G.page == i;
        if (sel) { rr(RGBA(C_TEAL, 38), RGBA(C_TEAL, 90), 1, t->x, t->y, t->w, t->h, 8); fillr(C_GOLD, t->x, t->y + 10, 4, t->h - 20); }
        else if (G.hover == i) rr(RGBA(0xFFFFFF, 14), 0, 0, t->x, t->y, t->w, t->h, 8);
        star4(t->x + 34, t->y + t->h / 2, 7, 4, sel ? C_GOLD : RGBA(C_MUT, 200));
        text(NAVS[i], t->x + 56, t->y, 150, t->h, F_NAV, sel ? C_TXT : 0xFFB3BCD8, 0, 1);
    }
    text("Not affiliated with Square Enix", 0, 556, 240, 18, F_SM, RGBA(C_MUT, 200), 1, 1);
    spaced("UNOFFICIAL NATIVE PC PORT", 120, 584, F_TAG, RGBA(C_GOLD, 200), 1.6f, 1);
    /* content */
    draw_banner(241, 0, 759, 150);
    static const char *T1[3] = { "Ready to play", "Graphics settings", "Controls" };
    static const char *T2[3] = { "Check that everything is in place, then press Play.", "Tune the picture to your screen. Changes save automatically.", "Your current keyboard layout." };
    text(T1[G.page], 280, 160, 680, 44, F_H1, C_TXT, 0, 1); text(T2[G.page], 280, 204, 680, 24, F_BODY, C_MUT, 0, 1);
    line_(0xFF1F2850, 1, 280, 244, 960, 244);
    if (G.page == 0) {
        for (int i = 0; i < 4; i++) {
            float y = 262 + i * 40; UINT c = G.ok[i] ? C_GREEN : C_RED;
            rr(C_PANEL, 0xFF1D2650, 1, 280, y, 680, 34, 8); rr(RGBA(c, 45), c, 1.5f, 292, y + 5, 24, 24, 12);
            if (G.ok[i]) { line_(c, 2.4f, 299, y + 17, 303, y + 21); line_(c, 2.4f, 303, y + 21, 310, y + 12); } else { line_(c, 2.4f, 299, y + 11, 309, y + 23); line_(c, 2.4f, 309, y + 11, 299, y + 23); }
            text(G.tx[i], 330, y, 620, 34, F_BODY, G.ok[i] ? C_TXT : 0xFFF0A0AC, 0, 1);
        }
        char hint[256] = "All set. Press Play to start.";
        if (!all_ready()) { static const char *nm[4] = { "Disc 1", "Disc 2", "BIOS", "the built game" }; int first = 1; strcpy(hint, "Cannot start yet. Missing: ");
            for (int i = 0; i < 4; i++) if (!G.ok[i]) { if (!first) strcat(hint, ", "); strcat(hint, nm[i]); first = 0; } strcat(hint, ". Run INSTALL.bat to fix this."); }
        text(hint, 280, 478, 680, 24, F_BODY, all_ready() ? C_GREEN : 0xFFF0A0AC, 0, 1);
    } else if (G.page == 1) {
        text(fs_mode() ? "Fullscreen uses your desktop resolution" : "", 280, 346, 320, 20, F_SM, C_GOLD, 2, 1);
        int dw = GetSystemMetrics(SM_CXSCREEN), dh = GetSystemMetrics(SM_CYSCREEN);
        text("Higher render resolution looks sharper but needs a faster graphics card.", 280, 494, 680, 20, F_SM, C_MUT, 0, 1);
        text("The game picture is 4:3, so on a 16:9 monitor you will see bars at the sides.", 280, 534, 680, 20, F_SM, C_MUT, 0, 1);
        text(fmt("Your desktop is %d x %d. Biggest window that fits: %s.", dw, dh, O_SIZE[best_size(dw, dh)].label), 280, 556, 680, 20, F_SM, C_MUT, 0, 1);
    } else {
        for (int i = 0, n = 0; i < NFIELDS; i++) {
            if (FIELDS[i].page != 1 || strncmp(FIELDS[i].key, "key.", 4)) continue;
            float x = 280 + (n / 8) * 350, y = 262 + (n % 8) * 36; const char *v = *get(FIELDS[i].key) ? get(FIELDS[i].key) : FIELDS[i].def; n++;
            text(FIELDS[i].label, x, y, 100, 30, F_BODY, 0xFFB3BCD8, 0, 1); rr(C_PANEL, 0xFF1D2650, 1, x + 110, y, 220, 30, 7); text(v, x + 122, y, 200, 30, F_BODY, C_TXT, 0, 1);
        }
        text("Change them in-game with F10, or edit settings.ini.", 280, 556, 680, 22, F_BODY, C_GOLD, 0, 1);
    }
    for (int i = 3; i < G.nit; i++) {
        const It *t = &G.it[i]; int hv = G.hover == i && t->en;
        if (t->kind == K_DD) {
            const Dd *d = &DDS[t->id]; UINT ac = t->en ? C_MUT : RGBA(C_MUT, 90);
            text(d->label, t->x, t->y - 24, t->w, 20, F_LAB, 0xFFB8C2E0, 0, 1);
            rr(hv ? 0xFF16214A : C_PANEL, hv ? C_TEAL : C_LINE, 1.3f, t->x, t->y, t->w, t->h, 7);
            text(d->o[dd_cur(t->id)].label, t->x + 14, t->y, t->w - 44, t->h, F_BODY, t->en ? C_TXT : RGBA(C_MUT, 150), 0, 1);
            line_(ac, 1.8f, t->x + t->w - 28, t->y + 16, t->x + t->w - 22, t->y + 22); line_(ac, 1.8f, t->x + t->w - 22, t->y + 22, t->x + t->w - 16, t->y + 16);
        } else if (t->kind == K_CHK) {
            float by = t->y + (t->h - 24) / 2; int on = chk_on(t->id);
            rr(on ? C_TEAL : C_PANEL, on ? C_TEAL : (hv ? C_TEAL : C_LINE), 1.5f, t->x, by, 24, 24, 6);
            if (on) { line_(C_BG, 2.8f, t->x + 6, by + 12.5f, t->x + 10.5f, by + 17); line_(C_BG, 2.8f, t->x + 10.5f, by + 17, t->x + 18.5f, by + 7); }
            text(t->id ? "Start the game right away next time (skip this window)" : "Scanlines (retro CRT look)", t->x + 36, t->y, t->w - 40, t->h, F_BODY, C_TXT, 0, 1);
        } else if (t->kind == K_PLAY) {
            if (t->en) rrgrad(t->x, t->y, t->w, t->h, 10, hv ? 0xFF58E8D8 : C_TEAL, hv ? 0xFF2DA4A8 : 0xFF22939A);
            else rr(0xFF1A2342, 0xFF2A3560, 1, t->x, t->y, t->w, t->h, 10);
            spaced("PLAY", t->x + t->w / 2, t->y + 14, F_BTN, t->en ? 0xFF06121C : RGBA(C_MUT, 160), 6, 1);
        }
    }
    /* footer */
    line_(0xFF1F2850, 1, 241, 598, 1000, 598);
    text("Changes save automatically and apply the next time you press Play.", 280, 600, 480, 40, F_SM, C_MUT, 0, 1);
    if (*G.toast) text(G.toast, 640, 600, 320, 40, F_BODY, G.toast_err ? C_RED : C_GREEN, 2, 1);
    if (G.focus >= 0 && G.focus < G.nit && G.it[G.focus].en) { const It *t = &G.it[G.focus]; rr(0, C_GOLD, 2, t->x - 3, t->y - 3, t->w + 6, t->h + 6, 9); }
    GdipDeleteGraphics(gg); gg = NULL;
}
static void gp_init(void) {
    ULONG_PTR tok; GsIn in = { 1, NULL, FALSE, FALSE }; GdiplusStartup(&tok, &in, NULL);
    static const struct { const wchar_t *fam; float sz; int style, serif; } FD[NF] = {
        { L"Segoe UI", 15, 0, 0 }, { L"Segoe UI", 13, 0, 0 }, { L"Georgia", 30, 0, 1 }, { L"Segoe UI", 17, 0, 0 }, { L"Georgia", 26, 1, 1 },
        { L"Georgia", 25, 1, 1 }, { L"Segoe UI", 10.5f, 1, 0 }, { L"Segoe UI", 14, 1, 0 }, { L"Georgia", 12.5f, 0, 1 } };
    for (int i = 0; i < NF; i++) {
        void *fam = NULL; if (GdipCreateFontFamilyFromName(FD[i].fam, NULL, &fam)) { if (FD[i].serif) GdipGetGenericFontFamilySerif(&fam); else GdipGetGenericFontFamilySansSerif(&fam); }
        GdipCreateFont(fam, FD[i].sz, FD[i].style, 2, &G.fnt[i]);
    }
    GdipCreateStringFormat(0, 0, &G.sf); GdipSetStringFormatFlags(G.sf, 0x1000);
    GdipStringFormatGetGenericTypographic(&G.tf); GdipSetStringFormatFlags(G.tf, 0x800 | 0x1000);
    char *b = fmt("%s\\launcher\\banner.png", G.root); if (!exists(b)) b = fmt("%s\\recomp\\launcher\\banner.png", G.root);
    void *img = NULL; UINT iw = 0, ih = 0;   /* missing or corrupt: silently keep the drawn banner */
    if (exists(b) && !GdipCreateBitmapFromFile(W(b), &img) && img && !GdipGetImageWidth(img, &iw) && !GdipGetImageHeight(img, &ih) && iw && ih) G.banner = img;
}
static int shot(const char *png, int page) {   /* --shot: render a page to a PNG without showing a window */
    G.s = 1; G.page = page; G.focus = 3; G.hover = -1; gp_init(); refresh();
    HDC sd = GetDC(NULL), dc = CreateCompatibleDC(sd); HBITMAP bm = CreateCompatibleBitmap(sd, 1000, 640); SelectObject(dc, bm);
    render(dc);
    void *img; static const CLSID png_clsid = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
    if (GdipCreateBitmapFromHBITMAP(bm, NULL, &img)) return 1;
    return GdipSaveImageToFile(img, W(png), &png_clsid, NULL) ? 1 : 0;
}
static void set_toast(const char *s, int err) { snprintf(G.toast, sizeof G.toast, "%s", s); G.toast_err = err; SetTimer(G.h, 1, 2500, NULL); }
static void save_key(const char *k, const char *v) {
    int r = ini_write(G.root, k, v);
    if (r == 1) set_toast("Saved", 0); else set_toast(r == -2 ? "Not saved: settings.ini is UTF-16" : "Not saved: cannot write settings.ini", 1);
    refresh(); InvalidateRect(G.h, NULL, FALSE);
}
static void dd_set(int d, int i) { if (i >= 0 && i < DDS[d].n && i != dd_cur(d)) save_key(DDS[d].key, DDS[d].o[i].val); }
static void activate(int i) {
    const It *t = &G.it[i]; if (!t->en) return; G.focus = i;
    if (t->kind == K_NAV) { G.page = t->id; G.hover = -1; InvalidateRect(G.h, NULL, FALSE); }
    else if (t->kind == K_CHK) save_key(t->id ? "skip_launcher" : "scanlines", chk_on(t->id) ? (t->id ? "0" : "off") : (t->id ? "1" : "on"));
    else if (t->kind == K_DD) {
        HMENU m = CreatePopupMenu(); int cur = dd_cur(t->id), d = t->id;
        for (int k = 0; k < DDS[d].n; k++) AppendMenuW(m, MF_STRING | (k == cur ? MF_CHECKED : 0), k + 1, W(DDS[d].o[k].label));
        POINT p = { (int)(t->x * G.s), (int)((t->y + t->h) * G.s) }; ClientToScreen(G.h, &p);
        int r = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, G.h, NULL); DestroyMenu(m);
        if (r > 0) dd_set(d, r - 1); else InvalidateRect(G.h, NULL, FALSE);
    } else if (t->kind == K_PLAY) {
        g_hide = G.h; int rc = 0;
        if (launch_game(G.root, &rc)) { G.rc = rc; DestroyWindow(G.h); }
        else { MessageBoxW(G.h, W(errmsg), L"Star Ocean: not ready", MB_OK | MB_ICONERROR); refresh(); InvalidateRect(G.h, NULL, FALSE); }
    }
}
static int hit(LPARAM lp) { float x = (short)LOWORD(lp) / G.s, y = (short)HIWORD(lp) / G.s; layout(); for (int i = G.nit - 1; i >= 0; i--) { const It *t = &G.it[i]; if (t->en && x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h) return i; } return -1; }
static void move_focus(int dir) { layout(); int from = G.focus < 0 ? (dir > 0 ? -1 : 0) : G.focus; for (int k = 1; k <= G.nit; k++) { int i = (from + dir * k + G.nit * 2) % G.nit; if (G.it[i].en) { G.focus = i; break; } } InvalidateRect(G.h, NULL, FALSE); }
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: G.h = h; refresh(); layout(); G.focus = G.nit > 3 ? 3 : 0; G.hover = -1; return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC md = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, rc.right, rc.bottom), old = SelectObject(md, bm);
        render(md); BitBlt(dc, 0, 0, rc.right, rc.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, old); DeleteObject(bm); DeleteDC(md); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: {
        if (!G.track) { TRACKMOUSEEVENT te = { sizeof te, TME_LEAVE, h, 0 }; TrackMouseEvent(&te); G.track = 1; }
        int i = hit(lp); if (i != G.hover) { G.hover = i; InvalidateRect(h, NULL, FALSE); } return 0; }
    case WM_MOUSELEAVE: G.track = 0; G.hover = -1; InvalidateRect(h, NULL, FALSE); return 0;
    case WM_SETCURSOR: if (LOWORD(lp) == HTCLIENT && G.hover >= 0) { SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND)); return TRUE; } break;
    case WM_LBUTTONDOWN: { int i = hit(lp); if (i >= 0) activate(i); return 0; }
    case WM_TIMER: if (wp == 1) { KillTimer(h, 1); G.toast[0] = 0; InvalidateRect(h, NULL, FALSE); } return 0;
    case WM_KEYDOWN: {
        layout(); const It *t = (G.focus >= 0 && G.focus < G.nit) ? &G.it[G.focus] : NULL;
        if (wp == VK_TAB) move_focus(GetKeyState(VK_SHIFT) < 0 ? -1 : 1);
        else if (wp == VK_DOWN) move_focus(1); else if (wp == VK_UP) move_focus(-1);
        else if ((wp == VK_LEFT || wp == VK_RIGHT) && t && t->kind == K_DD && t->en) dd_set(t->id, dd_cur(t->id) + (wp == VK_RIGHT ? 1 : -1));
        else if ((wp == VK_RETURN || wp == VK_SPACE) && t) activate(G.focus);
        return 0; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
static int run_gui(const char *root) {
    SetProcessDPIAware(); G.root = root;
    HDC sd = GetDC(NULL); G.s = GetDeviceCaps(sd, LOGPIXELSX) / 96.0f; ReleaseDC(NULL, sd); if (G.s < 1) G.s = 1;
    gp_init();
    HINSTANCE hi = GetModuleHandleW(NULL);
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = wndproc; wc.hInstance = hi; wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW); wc.hbrBackground = NULL;
    wc.lpszClassName = L"SO2Launcher"; wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1)); RegisterClassW(&wc);
    DWORD st = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX; RECT r = { 0, 0, (LONG)(1000 * G.s), (LONG)(640 * G.s) }; AdjustWindowRect(&r, st, FALSE);
    HWND h = CreateWindowW(L"SO2Launcher", L"Star Ocean: The Second Story", st | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    if (!h) return 1;
    MSG msg; while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return G.rc;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cl, int sw) {
    int argc; wchar_t **av = CommandLineToArgvW(GetCommandLineW(), &argc);
    int self = 0, only = 0, play = 0, pg = 0; char *root = NULL, *shotp = NULL;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(av[i], L"--selftest")) self = 1;
        else if (!wcscmp(av[i], L"--prepare")) only = 1;
        else if (!wcscmp(av[i], L"--play")) play = 1;
        else if (!wcscmp(av[i], L"--quiet")) quiet = 1;
        else if (!wcscmp(av[i], L"--root") && i + 1 < argc) root = U(av[++i]);
        else if (!wcscmp(av[i], L"--shot") && i + 1 < argc) shotp = U(av[++i]);   /* render a page to a PNG, no window (to check the look) */
        else if (!wcscmp(av[i], L"--page") && i + 1 < argc) pg = _wtoi(av[++i]) % 3;
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
    if (shotp) { G.root = root; return shot(shotp, pg); }
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
