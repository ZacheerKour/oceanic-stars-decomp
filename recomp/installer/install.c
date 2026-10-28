/* setup.exe: the installer program, compiled on the player's PC by INSTALL.bat from this file (no binary is tracked). It looks
 * for the two Star Ocean 2 (USA) discs in roms\ (or takes them as arguments / a file dialog), checks them by SHA-1,
 * fetches pinned, SHA-256-checked private copies of Python / MinGit / SDL3 / zlib, unpacks the game data, runs
 * recomp\rtfull\build_all.ps1 hidden and builds PLAYTHISGAME.exe (the launcher, recomp\launcher\play.c) at the repo root.
 * All tool output goes to install.log; the console shows short progress lines. Finished stages are skipped (stamps in .install\).
 * Contains no game code and no game data. Normally started by INSTALL.bat, which passes --root.
 * Flags: --selftest, --fetch <url> <sha256> <dest> (test the downloader), --root <dir>. Env: SO2_NOASK=1 (never pause or ask),
 * SO2_VERBOSE=1 (echo log tail after each tool). */
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/* ---------- helpers (short-lived process: nothing is freed) ---------- */
static char *fmt(const char *f, ...) {
    va_list a; va_start(a, f); int n = vsnprintf(NULL, 0, f, a); va_end(a);
    char *s = malloc(n + 1); va_start(a, f); vsnprintf(s, n + 1, f, a); va_end(a); return s;
}
static wchar_t *W(const char *s) { int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0); wchar_t *w = malloc(n * 2); MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n); return w; }
static char *U(const wchar_t *w) { int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL); char *s = malloc(n); WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL); return s; }
static char *lower(const char *s) { char *r = _strdup(s); for (char *c = r; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; return r; }
static int exists(const char *p) { return GetFileAttributesW(W(p)) != INVALID_FILE_ATTRIBUTES; }
static int is_dir(const char *p) { DWORD a = GetFileAttributesW(W(p)); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
static char *read_file(const char *p, size_t *len) {
    HANDLE f = CreateFileW(W(p), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD n = GetFileSize(f, NULL), got = 0; char *b = malloc(n + 1);
    if (!ReadFile(f, b, n, &got, NULL)) got = 0;
    CloseHandle(f); b[got] = 0; if (len) *len = got; return b;
}
static int write_file(const char *p, const void *d, size_t n) {
    HANDLE f = CreateFileW(W(p), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD got; int ok = WriteFile(f, d, (DWORD)n, &got, NULL) && got == n; CloseHandle(f); return ok;
}
static void mkdirs(const char *p) {
    char *s = _strdup(p);
    for (char *c = s + 1; ; c++) { if (*c == '\\' || *c == '/' || !*c) { char t = *c; *c = 0; CreateDirectoryW(W(s), NULL); if (!t) break; *c = t; } }
}
static char *parent(const char *p) { char *s = _strdup(p); char *c = strrchr(s, '\\'); if (c) *c = 0; return s; }
static const char *base(const char *p) { const char *c = strrchr(p, '\\'); return c ? c + 1 : p; }
static const char *ext_of(const char *p) { const char *b = base(p), *c = strrchr(b, '.'); return c ? c : ""; }

typedef struct { char **v; int n; } List;
static void push(List *l, const char *s) { l->v = realloc(l->v, (l->n + 1) * sizeof *l->v); l->v[l->n++] = _strdup(s); }
static int in_list(List *l, const char *s) { for (int i = 0; i < l->n; i++) if (!strcmp(l->v[i], s)) return 1; return 0; }

/* ---------- globals, console, log ---------- */
static char *root, *logp;
#define MAX_ROOT 120   /* the build tools (cmd, MSVC, git, PowerShell 5) are not long-path aware and nested build paths add ~120 characters */
static int con_out, noask, verbose;
static void say(const char *f, ...) {
    va_list a; va_start(a, f); int n = vsnprintf(NULL, 0, f, a); va_end(a);
    char *s = malloc(n + 2); va_start(a, f); vsnprintf(s, n + 1, f, a); va_end(a); s[n] = '\n'; s[n + 1] = 0;
    wchar_t *w = W(s); DWORD got;
    if (con_out) WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), w, (DWORD)wcslen(w), &got, NULL); else { fputs(s, stdout); fflush(stdout); }
}
static void log_add(const char *s) {
    HANDLE f = CreateFileW(W(logp), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return; DWORD got; WriteFile(f, s, (DWORD)strlen(s), &got, NULL); CloseHandle(f);
}
static void log_note(const char *m) { log_add(fmt("\r\n>>> %s\r\n", m)); }
static char *ascii(const char *s) { char *r = _strdup(s); for (char *c = r; *c; c++) if ((unsigned char)*c < 32 || (unsigned char)*c > 126) *c = '?'; return r; }
static void pause_if_alone(void) {   /* double-clicked: the console closes with the exe, so wait for Enter */
    DWORD pids[2]; if (noask || GetConsoleProcessList(pids, 2) > 1) return;
    say(""); say("Press Enter to close this window."); getchar();
}
static void tail_lines(int n) {
    size_t len; char *t = read_file(logp, &len); if (!t) return;
    char *p = t + len; int lines = 0;
    while (p > t && lines <= n) { p--; if (*p == '\n') lines++; }
    if (*p == '\n') p++;
    for (char *l = strtok(p, "\r\n"); l; l = strtok(NULL, "\r\n")) say("  %s", ascii(l));
}
static char *unpack_dir;   /* roms\_unpack while a disc is being unpacked: our own temp folder, removed on failure too */
static void fail(const char *f, ...) {
    if (unpack_dir && GetFileAttributesW(W(unpack_dir)) != INVALID_FILE_ATTRIBUTES) {
        wchar_t *c = W(fmt("cmd.exe /d /c rmdir /s /q \"%s\"", unpack_dir)); STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi;
        if (CreateProcessW(NULL, c, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) { WaitForSingleObject(pi.hProcess, 60000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    }
    va_list a; va_start(a, f); int n = vsnprintf(NULL, 0, f, a); va_end(a);
    char *m = malloc(n + 1); va_start(a, f); vsnprintf(m, n + 1, f, a); va_end(a);
    say(""); say("The install did not finish."); say("%s", m); log_note(m);
    if (exists(logp)) { say(""); say("Last lines of the log:"); tail_lines(15); say(""); say("Full log: %s", logp); }
    say("Run INSTALL.bat again after fixing the problem; finished stages are skipped.");
    pause_if_alone(); exit(1);
}
static int ask(const char *q) {
    if (noask) return 0;
    say("%s [Y/N]", q); char b[64]; if (!fgets(b, sizeof b, stdin)) return 0;
    char *c = b; while (*c == ' ' || *c == '\t') c++; return *c == 'y' || *c == 'Y';
}

/* ---------- hashing (bcrypt.dll, built into Windows) ---------- */
static char *hex(const unsigned char *d, int n) { char *s = malloc(n * 2 + 1); for (int i = 0; i < n; i++) sprintf(s + i * 2, "%02x", d[i]); return s; }
typedef struct { BCRYPT_ALG_HANDLE a; BCRYPT_HASH_HANDLE h; int len; } Hash;
static void hash_open(Hash *h, int bits) {
    h->len = bits == 1 ? 20 : 32;
    BCryptOpenAlgorithmProvider(&h->a, bits == 1 ? BCRYPT_SHA1_ALGORITHM : BCRYPT_SHA256_ALGORITHM, NULL, 0);
    BCryptCreateHash(h->a, &h->h, NULL, 0, NULL, 0, 0);
}
static char *hash_close(Hash *h) {
    unsigned char d[32]; BCryptFinishHash(h->h, d, h->len, 0); BCryptDestroyHash(h->h); BCryptCloseAlgorithmProvider(h->a, 0); return hex(d, h->len);
}
static char *hash_mem(const void *p, size_t n, int bits) { Hash h; hash_open(&h, bits); BCryptHashData(h.h, (PUCHAR)p, (ULONG)n, 0); return hash_close(&h); }
static char *hash_file(const char *path, int bits) {   /* NULL if unreadable */
    HANDLE f = CreateFileW(W(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    Hash h; hash_open(&h, bits); static unsigned char buf[1 << 20]; DWORD got;
    while (ReadFile(f, buf, sizeof buf, &got, NULL) && got) BCryptHashData(h.h, buf, got, 0);
    CloseHandle(f); return hash_close(&h);
}
static int verify(const char *path, const char *sha256) { char *h = hash_file(path, 256); return h && !strcmp(h, sha256); }

/* ---------- downloads (WinHTTP, SHA-256 pinned) ---------- */
static char *http_get(const char *url, const char *dest) {   /* returns an error text, or NULL on success */
    URL_COMPONENTS uc = { sizeof uc }; wchar_t host[256], path[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(W(url), 0, 0, &uc)) return "bad URL";
    HINTERNET s = WinHttpOpen(L"StarOcean2-INSTALL/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!s) return "WinHTTP unavailable";
    WinHttpSetTimeouts(s, 15000, 15000, 30000, 60000);
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0), r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, NULL, NULL, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : NULL;
    if (!r || !WinHttpSendRequest(r, NULL, 0, NULL, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL)) return "connection failed";
    DWORD code = 0, sz = sizeof code; WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &sz, NULL);
    if (code != 200) return fmt("server answered HTTP %lu", code);
    HANDLE f = CreateFileW(W(dest), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return "cannot write the file";
    static char buf[1 << 16]; DWORD got, w; char *err = NULL;
    for (;;) {
        if (!WinHttpReadData(r, buf, sizeof buf, &got)) { err = "download interrupted"; break; }
        if (!got) break;
        if (!WriteFile(f, buf, got, &w, NULL) || w != got) { err = "disk write failed"; break; }
    }
    CloseHandle(f); WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s); return err;
}
/* Fetches url to dest unless dest already holds the pinned content. A download is verified before it gets its final name. */
static void download(const char *url, const char *sha, const char *dest) {
    if (exists(dest)) {
        if (verify(dest, sha)) return;
        fail("The file %s does not match its expected SHA-256 fingerprint (%s), so it was not used. Delete that file and run INSTALL.bat again.", dest, sha);
    }
    mkdirs(parent(dest)); log_note(fmt("download %s", url));
    char *part = fmt("%s.part", dest), *e = http_get(url, part);
    if (e) fail("Could not download %s (%s). Check your internet connection.", url, e);
    if (!verify(part, sha)) { DeleteFileW(W(part)); fail("The download from %s does not match its expected SHA-256 fingerprint (%s), so it was discarded.", url, sha); }
    if (!MoveFileExW(W(part), W(dest), MOVEFILE_REPLACE_EXISTING)) fail("Could not move the finished download to %s (error %lu). Is the folder read-only or locked by antivirus?", dest, GetLastError());
}

/* ---------- processes ---------- */
static char *step_hint(void) {   /* last "== step" line a tool wrote to the log */
    HANDLE f = CreateFileW(W(logp), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return "";
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz); DWORD want = sz.QuadPart > 16384 ? 16384 : (DWORD)sz.QuadPart, got;
    LARGE_INTEGER at = { .QuadPart = sz.QuadPart - want }; SetFilePointerEx(f, at, NULL, FILE_BEGIN);
    char *b = malloc(want + 1); ReadFile(f, b, want, &got, NULL); CloseHandle(f); b[got] = 0;
    char *hint = ""; for (char *l = strtok(b, "\r\n"); l; l = strtok(NULL, "\r\n")) { if (!strncmp(l, ">>> ", 4)) hint = ""; else if (!strncmp(l, "== ", 3)) hint = l + 3; }
    return ascii(hint);
}
/* Runs a cmd.exe command line, hidden, all output appended to install.log; returns the exit code. */
static int run(const char *label, const char *cwd, const char *cmdline) {
    log_note(label);
    wchar_t *cmd = W(fmt("cmd.exe /d /s /c \"(%s) >> \"%s\" 2>&1 < nul\"", cmdline, logp));
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi; DWORD rc = 1;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, cwd ? W(cwd) : W(root), &si, &pi)) return 1;
    ULONGLONG t0 = GetTickCount64(); int i = 0; const char *spin = "|/-\\"; char *hint = "";
    while (WaitForSingleObject(pi.hProcess, 250) == WAIT_TIMEOUT) {
        if (i % 8 == 0) hint = step_hint();
        if (con_out) { ULONGLONG s = (GetTickCount64() - t0) / 1000; char *t = fmt("\r  %c %02d:%02d %s", spin[i % 4], (int)(s / 60), (int)(s % 60), hint); if (strlen(t) > 74) t[74] = 0; printf("%-74s", t); fflush(stdout); }
        i++;
    }
    if (con_out) { printf("\r%-76s\r", ""); fflush(stdout); }
    GetExitCodeProcess(pi.hProcess, &rc); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    if (verbose) tail_lines(40);
    return (int)rc;
}
static char *capture(const char *cmdline) {   /* stdout+stderr of a direct (non-cmd) process, trimmed; NULL if it cannot start */
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE r, w;
    if (!CreatePipe(&r, &w, &sa, 0)) return NULL;
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = w; si.hStdError = w; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessW(NULL, W(cmdline), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) { CloseHandle(r); CloseHandle(w); return NULL; }
    CloseHandle(w); char *out = malloc(1); size_t n = 0; char buf[1024]; DWORD got;
    while (ReadFile(r, buf, sizeof buf, &got, NULL) && got) { out = realloc(out, n + got + 1); memcpy(out + n, buf, got); n += got; }
    out[n] = 0; WaitForSingleObject(pi.hProcess, 60000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(r);
    while (n && (out[n - 1] == '\r' || out[n - 1] == '\n' || out[n - 1] == ' ')) out[--n] = 0;
    return out;
}
static void setenv_(const char *k, const char *v) { SetEnvironmentVariableW(W(k), W(v)); }
static char *getenv_(const char *k) {   /* live process environment: _wgetenv/getenv keep a startup copy that SetEnvironmentVariableW does not update */
    DWORD n = GetEnvironmentVariableW(W(k), NULL, 0); if (!n) return NULL;
    wchar_t *w = malloc(n * 2); DWORD got = GetEnvironmentVariableW(W(k), w, n); if (!got || got >= n) return NULL; return U(w);
}
static void path_prepend(const char *dir) { char *old = getenv_("PATH"); setenv_("PATH", fmt("%s;%s", dir, old ? old : "")); }
static int on_path(const wchar_t *exe) { wchar_t b[MAX_PATH]; return SearchPathW(NULL, exe, NULL, MAX_PATH, b, NULL) != 0; }

/* ---------- stamps: SHA-1 of the tracked inputs, so a git pull rebuilds only what changed ---------- */
static void walk(const char *rel, List *out) {
    char *pat = fmt("%s\\%s\\*", root, rel); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(W(pat), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char *n = U(fd.cFileName); if (!strcmp(n, ".") || !strcmp(n, "..") || n[0] == '_') continue;   /* generated _* folders are not inputs */
        char *r = fmt("%s\\%s", rel, n);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) walk(r, out); else if (_stricmp(ext_of(n), ".log")) push(out, r);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static int cmp(const void *a, const void *b) { return strcmp(*(char **)a, *(char **)b); }
static char *tree_stamp(const char **items, int ni, const char *extra) {
    List l = { 0 };
    for (int i = 0; i < ni; i++) { char *full = fmt("%s\\%s", root, items[i]); if (is_dir(full)) walk(items[i], &l); else push(&l, items[i]); }
    qsort(l.v, l.n, sizeof *l.v, cmp);
    size_t cap = 1024, n = 0; char *s = malloc(cap);
    for (int i = 0; i < l.n; i++) {
        char *h = hash_file(fmt("%s\\%s", root, l.v[i]), 1); char *line = fmt("%s:%s\n", l.v[i], h ? h : "none");
        size_t k = strlen(line); if (n + k + 1 > cap) { cap = (n + k) * 2; s = realloc(s, cap); } memcpy(s + n, line, k); n += k;
    }
    s = realloc(s, n + strlen(extra) + 1); strcpy(s + n, extra); return hash_mem(s, strlen(s), 1);
}
static int stamp_ok(const char *name, const char *v) {
    char *t = read_file(fmt("%s\\.install\\%s", root, name), NULL); if (!t) return 0;
    size_t n = strlen(t); while (n && (t[n - 1] == '\r' || t[n - 1] == '\n' || t[n - 1] == ' ')) t[--n] = 0; return !strcmp(t, v);
}
static void stamp_set(const char *name, const char *v) { write_file(fmt("%s\\.install\\%s", root, name), v, strlen(v)); }

/* ---------- discs ---------- */
static const char *DISC_SHA1[3] = { "", "ccd092233b353d2aa7bb80ecdb2b76f456430b41", "68178c55a430eab3e97eb51ec4a164d36f228aa1" };   /* recomp\game.toml known_sha1 */
static char *disc_name(int n) { return fmt("Star Ocean - The Second Story (USA) (Disc %d)", n); }
static int disc_ok(int n) { return exists(fmt("%s\\roms\\disc%d\\%s.cue", root, n, disc_name(n))); }
/* FILE "x" lines of a cue sheet (names only). */
static int cue_files(char *text, char **names, int max) {
    int n = 0;
    for (char *l = strtok(text, "\r\n"); l; l = strtok(NULL, "\r\n")) {
        while (*l == ' ' || *l == '\t') l++;
        if (_strnicmp(l, "FILE", 4) || (l[4] != ' ' && l[4] != '\t')) continue;
        char *q = strchr(l, '"'), *e = q ? strchr(q + 1, '"') : NULL;
        if (e) { *e = 0; if (n < max) names[n] = q + 1; n++; }
    }
    return n;
}
/* OPENFILENAME multi-select buffer: "path\0" alone is one file, "dir\0a\0b\0\0" is dir\a, dir\b. */
static int split_multiselect(const wchar_t *buf, List *out) {
    size_t l0 = wcslen(buf); const wchar_t *p = buf + l0 + 1;
    if (!*p) { push(out, U(buf)); return 1; }
    while (*p) { push(out, fmt("%s\\%s", U(buf), U(p))); p += wcslen(p) + 1; }
    return out->n;
}
static void pick_files(const char *title, List *out) {
    static wchar_t buf[32768]; OPENFILENAMEW o = { sizeof o };
    o.lpstrTitle = W(title); o.lpstrFile = buf; o.nMaxFile = 32768; o.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    o.lpstrFilter = L"Star Ocean disc files\0*.7z;*.zip;*.cue;*.bin;*.chd;*.iso\0All files\0*.*\0";
    if (GetOpenFileNameW(&o)) split_multiselect(buf, out);
}
static void collect(const char *dir, List *out) {   /* .bin/.chd files below dir */
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(W(fmt("%s\\*", dir)), &fd); if (h == INVALID_HANDLE_VALUE) return;
    do {
        char *n = U(fd.cFileName); if (!strcmp(n, ".") || !strcmp(n, "..")) continue; char *p = fmt("%s\\%s", dir, n);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) collect(p, out); else { char *e = lower(ext_of(n)); if (!strcmp(e, ".bin") || !strcmp(e, ".chd")) push(out, p); }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static char *tools_dl(const char *name) { return fmt("%s\\tools\\dl\\%s", root, name); }
static char *seven_zip(void) {
    const char *pf = getenv("ProgramFiles"), *pf86 = getenv("ProgramFiles(x86)");
    char *c[3] = { fmt("%s\\7-Zip\\7z.exe", pf ? pf : ""), fmt("%s\\7-Zip\\7z.exe", pf86 ? pf86 : ""), fmt("%s\\tools\\7zr.exe", root) };
    for (int i = 0; i < 3; i++) if (exists(c[i])) return c[i];
    download("https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe", "9bf582b894dfa665f852d7e9112b02290a1c303c2d95a21fcdc41ffcb03143b7", c[2]); return c[2];
}
static char *tar_exe(void) { return fmt("%s\\System32\\tar.exe", getenv("SystemRoot") ? getenv("SystemRoot") : "C:\\Windows"); }

static void scan_roms(List *out) {   /* disc files lying directly in roms\ (archives, .bin/.cue/.chd; not the BIOS) */
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(W(fmt("%s\\roms\\*", root)), &fd); if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char *n = U(fd.cFileName), *e = lower(ext_of(n)), *l = lower(n);
        if (!strncmp(l, "scph", 4)) continue;
        if (!strcmp(e, ".7z") || !strcmp(e, ".zip") || !strcmp(e, ".cue") || !strcmp(e, ".bin") || !strcmp(e, ".chd") || !strcmp(e, ".iso")) push(out, fmt("%s\\roms\\%s", root, n));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static void disc_help(void) {
    say("  Put BOTH Star Ocean 2 (USA) disc images into this folder, then run INSTALL.bat again:");
    say("    %s\\roms", root);
    say("  Accepted: .7z or .zip archives, or .bin (+ .cue) files, any names, any order (see roms\\README.txt).");
    say("  Or drag the two disc files onto INSTALL.bat.");
}
static void step_discs(List *given) {
    if (disc_ok(1) && disc_ok(2)) { say("  Both discs are already installed."); return; }
    int interactive = given->n == 0;
    if (interactive) {
        scan_roms(given);
        if (given->n) say("  Found %d file(s) in roms\\.", given->n);
        else { say("  No disc files found in roms\\."); disc_help(); if (noask) fail("No disc files found. Put both disc images in %s\\roms and run INSTALL.bat again.", root); say("  Opening a file picker as a fallback..."); }
    }
    char *tmp = fmt("%s\\roms\\_unpack", root); int attempt = 0, k = 0; unpack_dir = tmp;
    while (!(disc_ok(1) && disc_ok(2))) {
        attempt++;
        if (given->n == 0) {
            if (!interactive || attempt > 3 || noask) break;
            int have = disc_ok(1) ? 1 : disc_ok(2) ? 2 : 0;
            char *t = have ? fmt("Select the file for disc %d", 3 - have) : "Select BOTH Star Ocean 2 disc files (hold Ctrl to pick two)";
            say("  %s...", t); pick_files(t, given);
            if (!given->n) { disc_help(); fail("No disc files were selected."); }
        }
        List images = { 0 };
        for (int i = 0; i < given->n; i++) {
            char *g = given->v[i], *e = lower(ext_of(g)); k++;
            if (!exists(g)) fail("Cannot find '%s'. Drag the disc files themselves onto INSTALL.bat.", g);
            if (is_dir(g)) fail("'%s' is a folder. Drag the two disc FILES (.7z, .zip, .bin/.cue or .chd) onto INSTALL.bat, not a folder.", base(g));
            if (!strcmp(e, ".7z") || !strcmp(e, ".zip")) {
                char *dir = fmt("%s\\a%d", tmp, k); say("  Unpacking %s...", ascii(base(g))); mkdirs(dir);
                int rc = !strcmp(e, ".zip") ? run("tar x zip", NULL, fmt("\"%s\" -xf \"%s\" -C \"%s\"", tar_exe(), g, dir))
                                           : run("7z x", NULL, fmt("\"%s\" x -y \"-o%s\" \"%s\"", seven_zip(), dir, g));
                if (rc) fail("Could not unpack %s. The file may be damaged or incomplete.", g);
                collect(dir, &images);
            } else if (!strcmp(e, ".cue")) {
                char *names[8]; char *t = read_file(g, NULL); int n = t ? cue_files(t, names, 8) : 0;
                if (n != 1) fail("%s lists %d data files. Only single-file disc images (one .bin per disc) are supported.", base(g), n);
                char *b = fmt("%s\\%s", parent(g), names[0]);
                if (!exists(b)) fail("%s points at '%s', which is not next to it.", base(g), names[0]);
                push(&images, b);
            } else if (!strcmp(e, ".bin") || !strcmp(e, ".chd")) push(&images, g);
            else if (!strcmp(e, ".iso")) fail("%s is a 2048-byte ISO. The game needs the raw (2352-byte) .bin dump of each disc, not an ISO.", base(g));
            else fail("%s is not a disc file I understand. Use the .7z, .zip, .bin/.cue or .chd of each disc.", base(g));
        }
        int seen[3] = { 0 }; List uniq = { 0 };
        for (int i = 0; i < images.n; i++) if (!in_list(&uniq, lower(images.v[i]))) push(&uniq, lower(images.v[i])), images.v[uniq.n - 1] = images.v[i];
        for (int i = 0; i < uniq.n; i++) {
            char *im = images.v[i]; const char *name = base(im);
            if (!strcmp(lower(ext_of(im)), ".chd")) {
                wchar_t cm[MAX_PATH]; DWORD cl = SearchPathW(NULL, L"chdman.exe", NULL, MAX_PATH, cm, NULL); if (!cl || cl >= MAX_PATH)
                    fail("%s is a CHD file and converting it needs chdman.exe (part of MAME), which was not found. Convert it to .bin/.cue first, or use the .7z/.bin files.", name);
                char *d2 = fmt("%s\\chd%d", tmp, ++k); mkdirs(d2); say("  Converting %s...", ascii(name));
                if (run("chdman", NULL, fmt("\"%s\" extractcd -i \"%s\" -o \"%s\\d.cue\" -ob \"%s\\d.bin\"", U(cm), im, d2, d2))) fail("chdman could not convert %s.", name);
                im = fmt("%s\\d.bin", d2);
            }
            say("  Checking %s...", ascii(name)); char *h = hash_file(im, 1); int n = !h ? 0 : !strcmp(h, DISC_SHA1[1]) ? 1 : !strcmp(h, DISC_SHA1[2]) ? 2 : 0;
            if (!n) { say("  '%s' is not a supported Star Ocean 2 disc. Use the unmodified USA dumps (SCUS-94421 and SCUS-94422); other regions and edited images do not work.", ascii(name)); continue; }
            if (seen[n] || disc_ok(n)) { say("  '%s' is disc %d, which you already gave. Disc %d is the other one.", ascii(name), n, 3 - n); continue; }
            seen[n] = 1; char *d = fmt("%s\\roms\\disc%d", root, n), *dn = disc_name(n), *dst = fmt("%s\\%s.bin", d, dn); mkdirs(d);
            if (_strnicmp(im, tmp, strlen(tmp)) ? !CopyFileW(W(im), W(dst), FALSE) : !MoveFileExW(W(im), W(dst), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) fail("Could not copy %s into roms\\disc%d (disk full?).", name, n);
            char *cue = fmt("FILE \"%s.bin\" BINARY\r\n  TRACK 01 MODE2/2352\r\n    INDEX 01 00:00:00\r\n", dn);   /* single-track MODE2/2352 image */
            write_file(fmt("%s\\%s.cue", d, dn), cue, strlen(cue)); say("  Disc %d recognised.", n);
        }
        if (exists(tmp)) run("cleanup unpack dir", NULL, fmt("rmdir /s /q \"%s\"", tmp));
        given->n = 0; if (!interactive) break;
    }
    if (exists(tmp)) run("cleanup unpack dir", NULL, fmt("rmdir /s /q \"%s\"", tmp));
    unpack_dir = NULL;
    for (int n = 1; n <= 2; n++) if (!disc_ok(n)) { disc_help(); fail("Disc %d was not found among the files you gave. Star Ocean 2 (USA) has two discs; give me both (any order), as .7z or .bin/.cue files.", n); }
}

/* ---------- tools ---------- */
static const char CMAKE_COMPONENT[] = "Microsoft.VisualStudio.Component.VC.CMake.Project";
static const char *vs_partial;   /* set by find_vs: an install that has the C++ compiler but not the CMake component */
static int has_cl(const char *vs) { return exists(fmt("%s\\VC\\Auxiliary\\Build\\vcvars64.bat", vs)); }
static int has_cmake(const char *vs) { return exists(fmt("%s\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe", vs)); }
static char *find_vs(void) {   /* installation path with MSVC + the CMake component (the same test INSTALL.bat makes), else NULL */
    vs_partial = NULL;
    const char *pf86 = getenv("ProgramFiles(x86)"); char *vw = fmt("%s\\Microsoft Visual Studio\\Installer\\vswhere.exe", pf86 ? pf86 : "C:\\Program Files (x86)");
    if (exists(vw)) {
        path_prepend(parent(vw));   /* vcvars64.bat and build_all.ps1 need vswhere on PATH */
        char *o = capture(fmt("\"%s\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 %s -property installationPath", vw, CMAKE_COMPONENT));
        if (o && *o && has_cl(o) && has_cmake(o)) return o;
        o = capture(fmt("\"%s\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath", vw));
        if (o && *o && has_cl(o)) vs_partial = o;
    }
    char *bt = fmt("%s\\Microsoft Visual Studio\\2022\\BuildTools", pf86 ? pf86 : "C:\\Program Files (x86)");
    if (has_cl(bt)) { if (has_cmake(bt)) return bt; if (!vs_partial) vs_partial = bt; }
    return NULL;
}
#define MINGIT_SHA "50b04b55425b5c465d076cdb184f63a0cd0f86f6ec8bb4d5860114a713d2c29a"
#define PYTHON_SHA "009d6bf7e3b2ddca3d784fa09f90fe54336d5b60f0e0f305c37f400bf83cfd3b"
static char *step_tools(void) {
    char *vs = find_vs();
    if (!vs && vs_partial) {
        const char *pf86 = getenv("ProgramFiles(x86)");
        say("  Visual Studio is installed (%s) but it lacks the \"C++ CMake tools for Windows\" component, which the build needs.", vs_partial);
        if (!ask("  Add that component now? Windows will ask for your approval."))
            fail("The \"C++ CMake tools for Windows\" component is missing. Open Visual Studio Installer, choose Modify, tick that component, then run INSTALL.bat again.");
        if (run("add cmake component", NULL, fmt("\"%s\\Microsoft Visual Studio\\Installer\\setup.exe\" modify --installPath \"%s\" --add %s --passive --wait --norestart", pf86 ? pf86 : "C:\\Program Files (x86)", vs_partial, CMAKE_COMPONENT)))
            fail("Adding the CMake component failed or was cancelled. Open Visual Studio Installer, choose Modify, tick \"C++ CMake tools for Windows\", then run INSTALL.bat again.");
        vs = find_vs(); if (!vs) fail("The CMake component is still missing after the change (a restart may be needed). Restart Windows if asked, then run INSTALL.bat again.");
    }
    if (!vs) {
        /* shortcut: no own MSVC download (plan option 2); winget Build Tools behind a Y/N, see the follow-up note in docs/notes */
        if (!on_path(L"winget.exe")) fail("The Visual Studio C++ compiler is needed but winget is not available. Install the Visual Studio 2022 Build Tools with the \"Desktop development with C++\" workload yourself (see README), then run INSTALL.bat again.");
        if (!ask("  Visual Studio 2022 Build Tools (C++) are needed (about 8 GB). Install them now with winget? Windows will ask for your approval."))
            fail("The Visual Studio C++ compiler is needed to build the game. Install it (see README) and run INSTALL.bat again.");
        say("  Installing Build Tools (this can take a while)...");
        if (run("winget BuildTools", NULL, "winget install --id Microsoft.VisualStudio.2022.BuildTools -e --source winget --accept-package-agreements --accept-source-agreements --override \"--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended\""))
            fail("Installing the Build Tools failed or was cancelled.");
        vs = find_vs(); if (!vs) fail("The Build Tools install did not finish (a restart may be needed). Restart Windows if asked, then run INSTALL.bat again.");
    }
    setenv_("SO2_VS", vs); log_note(fmt("compiler: %s", vs));
    if (!on_path(L"git.exe")) {   /* private MinGit, no admin and no winget needed */
        char *dir = fmt("%s\\tools\\mingit", root);
        if (!exists(fmt("%s\\cmd\\git.exe", dir)) || !stamp_ok("mingit.stamp", MINGIT_SHA)) {   /* stamp = unpack finished; git.exe alone can be left by a killed unpack */
            say("  Fetching a private Git..."); char *zip = tools_dl("MinGit-2.47.1-64-bit.zip"); mkdirs(dir);
            download("https://github.com/git-for-windows/git/releases/download/v2.47.1.windows.1/MinGit-2.47.1-64-bit.zip", MINGIT_SHA, zip);
            if (run("unpack mingit", NULL, fmt("\"%s\" -xf \"%s\" -C \"%s\"", tar_exe(), zip, dir))) fail("Could not unpack the private Git.");
            stamp_set("mingit.stamp", MINGIT_SHA);
        }
        path_prepend(fmt("%s\\cmd", dir));
    }
    char *py = fmt("%s\\tools\\python", root);   /* private Python 3.11.9 embeddable, SHA-256 checked */
    if (!exists(fmt("%s\\python.exe", py)) || !stamp_ok("python.stamp", PYTHON_SHA)) {
        say("  Fetching a private Python..."); char *zip = tools_dl("python-embed.zip"); mkdirs(py);
        download("https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip", PYTHON_SHA, zip);
        if (run("unpack python", NULL, fmt("\"%s\" -xf \"%s\" -C \"%s\"", tar_exe(), zip, py))) fail("Could not unpack the private Python.");
        stamp_set("python.stamp", PYTHON_SHA);
    }
    /* the embeddable build's ._pth stops scripts importing sibling modules; without it Python behaves normally */
    char *pth = fmt("%s\\python311._pth", py); if (exists(pth) && !MoveFileExW(W(pth), W(fmt("%s.off", pth)), MOVEFILE_REPLACE_EXISTING)) fail("Could not prepare the private Python (error %lu).", GetLastError());
    path_prepend(py); setenv_("SO2_DATA", root); setenv_("PYTHONUTF8", "1");
    return vs;
}

/* ---------- psxrecomp: pinned clone ---------- */
static void note_cleanup(const char *line) {   /* nothing is ever deleted for the player: leftovers are listed here */
    char *p = fmt("%s\\.install\\CLEANUP_LATER.md", root), *old = read_file(p, NULL);
    char *t = fmt("%s- %s\r\n", old && *old ? old : "# Safe to delete later (the installer never deletes these itself)\r\n", line); write_file(p, t, strlen(t));
}
static char *clone_head(const char *px) {   /* HEAD commit of a usable clone, else NULL (missing, or half-made by an interrupted clone) */
    if (!is_dir(fmt("%s\\.git", px))) return NULL;   /* --git-dir below: never let git walk up into another repository */
    char *h = capture(fmt("git --git-dir=\"%s\\.git\" rev-parse --verify -q HEAD^{commit}", px));
    return h && strlen(h) == 40 && strspn(h, "0123456789abcdef") == 40 ? h : NULL;
}
static void step_psxrecomp(const char *px, const char *commit) {
    char *head = clone_head(px);
    if (!head && exists(px)) {
        int n = 1; char *b; while (exists(b = fmt("%s_broken_%d", px, n))) n++;
        say("  The earlier psxrecomp download is incomplete. Keeping it as %s and downloading again.", base(b));
        if (!MoveFileW(W(px), W(b))) fail("Could not rename the incomplete folder %s (error %lu). Close programs using it or rename it yourself, then run INSTALL.bat again.", px, GetLastError());
        note_cleanup(fmt("tools\\ref\\%s (incomplete psxrecomp download)", base(b)));
    }
    if (!head) {
        say("  Downloading psxrecomp...");
        if (run("clone psxrecomp", NULL, fmt("git clone -q --filter=blob:none --no-checkout https://github.com/RetroPortingToolKit/psxrecomp \"%s\"", px)))
            fail("Could not download psxrecomp from github.com. Check your internet connection.");
        if (!(head = clone_head(px))) fail("The psxrecomp download finished but the copy is unusable (see the log).");
    }
    if (strcmp(head, commit)) {
        char *t = capture(fmt("git --git-dir=\"%s\\.git\" cat-file -t %s", px, commit));
        if (!t || strcmp(t, "commit")) {
            say("  The saved psxrecomp copy lacks the pinned version %.8s. Fetching it...", commit);
            if (run("fetch psxrecomp pin", px, fmt("git fetch -q origin %s", commit)) && run("fetch psxrecomp all", px, "git fetch -q --all --tags"))
                fail("Could not fetch the pinned psxrecomp version %s from github.com. Check your internet connection.", commit);
        }
        if (run("checkout psxrecomp", NULL, fmt("git -C \"%s\" -c advice.detachedHead=false checkout -q %s", px, commit))) fail("Could not check out the pinned psxrecomp version %s.", commit);
    }
}

/* ---------- icon for PLAYTHISGAME.exe (generated, so no binary asset is tracked) ---------- */
static int in_star(double x, double y) {   /* 5-point star, outer radius .86, inner .36, point up; even-odd ray cast */
    double px[10], py[10]; const double PI = 3.14159265358979;
    for (int i = 0; i < 10; i++) { double r = i % 2 ? .36 : .86, a = -PI / 2 + i * PI / 5; px[i] = r * cos(a); py[i] = r * sin(a); }
    int in = 0; for (int i = 0, j = 9; i < 10; j = i++) if ((py[i] > y) != (py[j] > y) && x < (px[j] - px[i]) * (y - py[i]) / (py[j] - py[i]) + px[i]) in = !in;
    return in;
}
static void write_ico(const char *path) {
    static const int sz[4] = { 16, 32, 48, 256 }; size_t cap = 6 + 16 * 4, total = cap, off[4], len[4];
    for (int i = 0; i < 4; i++) { len[i] = 40 + sz[i] * sz[i] * 4 + (size_t)((sz[i] + 31) / 32 * 4) * sz[i]; off[i] = total; total += len[i]; }
    unsigned char *b = calloc(total, 1); b[2] = 1; b[4] = 4;
    for (int i = 0; i < 4; i++) {
        unsigned char *e = b + 6 + 16 * i; e[0] = e[1] = sz[i] == 256 ? 0 : (unsigned char)sz[i]; e[4] = 1; e[6] = 32; *(DWORD *)(e + 8) = (DWORD)len[i]; *(DWORD *)(e + 12) = (DWORD)off[i];
        unsigned char *h = b + off[i]; int n = sz[i]; *(DWORD *)h = 40; *(LONG *)(h + 4) = n; *(LONG *)(h + 8) = 2 * n; *(WORD *)(h + 12) = 1; *(WORD *)(h + 14) = 32;
        unsigned char *px = h + 40, *mask = px + n * n * 4; int mrow = (n + 31) / 32 * 4;
        for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
            double fx = (x + .5) / n * 2 - 1, fy = (y + .5) / n * 2 - 1; int row = n - 1 - y;   /* bottom-up */
            unsigned char *p = px + (row * n + x) * 4;
            if (fx * fx + fy * fy > .96) { mask[row * mrow + x / 8] |= 0x80 >> (x % 8); continue; }   /* outside the disc: transparent */
            if (in_star(fx, fy + .06)) { p[0] = 60; p[1] = 200; p[2] = 245; } else { p[0] = 120 - (BYTE)(40 * fy); p[1] = 40; p[2] = 25; }   /* BGR: gold star on deep blue */
            p[3] = 255;
        }
    }
    write_file(path, b, total);
}

/* ---------- main flow ---------- */
static int selftest(void);
static int fetch_cmd(wchar_t **av, int ac, int i) {
    if (i + 3 >= ac) { fprintf(stderr, "usage: --fetch url sha256 dest\n"); return 2; }
    download(U(av[i + 1]), U(av[i + 2]), U(av[i + 3])); say("fetched and verified: %s", U(av[i + 3])); return 0;
}
int wmain(int argc, wchar_t **argv) {
    con_out = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &(DWORD){ 0 }) != 0;
    noask = getenv("SO2_NOASK") != NULL; verbose = getenv("SO2_VERBOSE") != NULL;
    List given = { 0 }; char *r = NULL; int sync_only = 0;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--root") && i + 1 < argc) r = U(argv[++i]);
        else if (!wcscmp(argv[i], L"--selftest")) return selftest();
        else if (!wcscmp(argv[i], L"--sync-psxrecomp")) sync_only = 1;
        else if (!wcscmp(argv[i], L"--fetch")) { root = "."; logp = "install.log"; return fetch_cmd(argv, argc, i); }
        else push(&given, U(argv[i]));
    }
    if (!r) { static wchar_t d[32768]; DWORD n = GetModuleFileNameW(NULL, d, 32768); wchar_t *sl = n && n < 32768 ? wcsrchr(d, L'\\') : NULL; if (!sl) { fprintf(stderr, "Cannot find the folder this program is in. Start INSTALL.bat instead.\n"); return 1; } *sl = 0; r = U(d); }
    path_prepend(fmt("%s\\System32", getenv("SystemRoot") ? getenv("SystemRoot") : "C:\\Windows"));   /* build_all.ps1 calls bare `tar`: use bsdtar, not a GNU tar from Git Bash/MSYS */
    root = r;
    if (strlen(root) > MAX_ROOT) { fprintf(stderr, "The folder path is too long (%d characters, the limit is %d): %s\nThe compiler tools used for the build cannot handle long paths. Move the game folder to a short path such as C:\\StarOcean2 and run INSTALL.bat again.\n", (int)strlen(root), MAX_ROOT, root); if (!noask && con_out) system("pause"); return 1; }
    SetCurrentDirectoryW(W(root)); logp = fmt("%s\\install.log", root); mkdirs(fmt("%s\\.install", root));
    char *hdr = fmt("Star Ocean 2 install log (INSTALL.bat)\r\n"); write_file(logp, hdr, strlen(hdr));
    const char *commit = "f42d2c004b3dd6ef6ccb83101b40ede55f24bf59";   /* audited psxrecomp pin (RECOMP_NOTES '2026-10-26 psxrecomp safety audit') */
    say("Star Ocean: The Second Story - PC install"); say("A detailed log is written to install.log."); say("");
    if (!exists(fmt("%s\\recomp\\rtfull\\build_all.ps1", root))) fail("This folder does not look like the game repository (recomp\\rtfull\\build_all.ps1 is missing). Keep INSTALL.bat at the top of the cloned folder.");

    if (sync_only) { step_tools(); step_psxrecomp(fmt("%s\\tools\\ref\\psxrecomp", root), commit); say("psxrecomp is at the pinned version."); return 0; }   /* test hook */
    say("[1/6] Checking your discs..."); step_discs(&given);
    say("[2/6] Checking tools..."); char *vs = step_tools();

    say("[3/6] Reading game data from the discs...");
    char *d1 = fmt("roms\\disc1\\%s.bin", disc_name(1));
    char *xh = hash_file(fmt("%s\\analysis\\extract.py", root), 1), *bootm = fmt("boot %s %s", DISC_SHA1[1], hash_file(fmt("%s\\analysis\\extract_boot.py", root), 1)), *extm = fmt("extract %s %s %s", DISC_SHA1[1], DISC_SHA1[2], xh);
    if (!exists(fmt("%s\\extracted\\SCUS_944.21", root)) || !stamp_ok("boot.stamp", bootm)) {   /* stamps are written only after the step succeeded */
        mkdirs(fmt("%s\\extracted", root));
        if (run("boot exe", NULL, fmt("\"%s\\tools\\python\\python.exe\" -I analysis\\extract_boot.py \"%s\" extracted\\SCUS_944.21", root, d1))) fail("Could not read the boot program from disc 1.");
        stamp_set("boot.stamp", bootm);
    }
    if (!exists(fmt("%s\\analysis\\out\\d1\\0001_312.dec", root)) || !stamp_ok("extract.stamp", extm)) {
        if (run("extract", NULL, fmt("\"%s\\tools\\python\\python.exe\" -E -s analysis\\extract.py", root))) fail("Could not unpack the game archive from the discs. The disc images may be damaged.");
        stamp_set("extract.stamp", extm);
    }

    say("[4/6] Downloading build components (checked against fixed fingerprints)...");
    char *px = fmt("%s\\tools\\ref\\psxrecomp", root);
    step_psxrecomp(px, commit);
    download("https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.tar.gz", "12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785", tools_dl("SDL3-3.4.10.tar.gz"));
    download("https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz", "9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23", tools_dl("zlib-1.3.1.tar.gz"));

    const char *bd[] = { "recomp\\aot", "recomp\\bios", "recomp\\host", "recomp\\rtfull", "recomp\\game.toml", "analysis", "config" };
    char *bios = exists(fmt("%s\\roms\\scph5501.bin", root)) ? "ownbios" : "openbios", *bstamp = tree_stamp(bd, 7, fmt("%s %s", commit, bios));
    char *exe = fmt("%s\\recomp\\rtfull\\_build_final\\psxrecomp-game.exe", root);
    if (exists(exe) && stamp_ok("exe_build.stamp", bstamp)) say("[5/6] Game code is up to date.");
    else {
        say("[5/6] Building game code (about 10 minutes)...");
        if (run("build_all", NULL, "powershell -NoProfile -ExecutionPolicy Bypass -File \"recomp\\rtfull\\build_all.ps1\""))
            fail("Building the game code failed. The log below usually names the cause (common ones: Build Tools missing the C++ workload, low disk space, antivirus blocking the compiler).");
        if (!exists(exe)) fail("The build finished without producing the game program.");
        stamp_set("exe_build.stamp", bstamp);
    }

    const char *ld[] = { "recomp\\launcher" }; char *lstamp = tree_stamp(ld, 1, "launcher1"), *out = fmt("%s\\PLAYTHISGAME.exe", root);
    if (exists(out) && stamp_ok("exe_launcher.stamp", lstamp)) say("[6/6] PLAYTHISGAME.exe is up to date.");
    else {
        say("[6/6] Building PLAYTHISGAME.exe..."); char *ldir = fmt("%s\\recomp\\launcher", root); mkdirs(fmt("%s\\_build", ldir)); write_ico(fmt("%s\\_build\\star.ico", ldir));
        char *cl = fmt("call \"%s\\VC\\Auxiliary\\Build\\vcvars64.bat\" >nul && rc /nologo /fo _build\\play.res play.rc && cl /nologo /O2 /MT /GS- /utf-8 /wd4996 /Fo_build\\ \"/Fe%s\" play.c _build\\play.res /link /MANIFEST:NO /SUBSYSTEM:WINDOWS user32.lib kernel32.lib shell32.lib", vs, out);
        if (run("PLAYTHISGAME.exe", ldir, cl) || !exists(out)) fail("Building PLAYTHISGAME.exe failed.");
        if (run("PLAYTHISGAME selftest", NULL, fmt("\"%s\" --selftest", out))) fail("PLAYTHISGAME.exe was built but its self-test failed.");
        stamp_set("exe_launcher.stamp", lstamp);
    }

    /* optional wide-screen side pictures from the player's own discs; never fails the install */
    say("Game art for wide screens: %s", run("side art", NULL, fmt("\"%s\\tools\\python\\python.exe\" -I scripts\\extract_side_art.py --roms roms", root)) ? "skipped" : "done");
    say(""); say("Done. Double-click PLAYTHISGAME.exe:"); say("  %s", out);
    if (con_out && !noask && ask("Create a Desktop shortcut?")) {
        wchar_t desk[MAX_PATH]; IShellLinkW *sl = NULL; IPersistFile *pf = NULL; int ok = 0;
        CoInitialize(NULL);
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk)) && SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl))) {
            sl->lpVtbl->SetPath(sl, W(out)); sl->lpVtbl->SetWorkingDirectory(sl, W(root));
            if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) { ok = SUCCEEDED(pf->lpVtbl->Save(pf, W(fmt("%s\\Star Ocean 2.lnk", U(desk))), TRUE)); pf->lpVtbl->Release(pf); }
            sl->lpVtbl->Release(sl);
        }
        say(ok ? "  Shortcut created on the Desktop." : "  Could not create the shortcut; start PLAYTHISGAME.exe from the folder above.");
    }
    say(""); say("Steam: Games > Add a Non-Steam Game to My Library > Browse > pick PLAYTHISGAME.exe > Add.");
    say("Steam sets Start In to this folder; the exe waits for the game, so Steam tracks it.");
    pause_if_alone(); return 0;
}

/* ---------- self-test ---------- */
static int tfail;
#define CHECK(c) do { if (!(c)) { tfail++; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); } } while (0)
static int selftest(void) {
    CHECK(!strcmp(hash_mem("abc", 3, 1), "a9993e364706816aba3e25717850c26c9cd0d89d"));
    CHECK(!strcmp(hash_mem("abc", 3, 256), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(!strcmp(hash_mem("", 0, 1), "da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); char *f = fmt("%sso2_install_selftest.bin", U(tmp));
    write_file(f, "abc", 3); CHECK(verify(f, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(!verify(f, "0000000000000000000000000000000000000000000000000000000000000000")); CHECK(!verify(fmt("%s.missing", f), "00"));
    char *names[8]; char c1[] = "FILE \"a b.bin\" BINARY\r\n  TRACK 01 MODE2/2352\r\n    INDEX 01 00:00:00\r\n"; CHECK(cue_files(c1, names, 8) == 1 && !strcmp(names[0], "a b.bin"));
    char c2[] = "FILE \"t1.bin\" BINARY\nTRACK 01 AUDIO\n  file \"t2.bin\" BINARY\n"; CHECK(cue_files(c2, names, 8) == 2);
    char c3[] = "REM FILE nothing\nTRACK 01\n"; CHECK(cue_files(c3, names, 8) == 0);
    List l = { 0 }; wchar_t m1[] = L"C:\\x\\disc.7z\0\0"; CHECK(split_multiselect(m1, &l) == 1 && !strcmp(l.v[0], "C:\\x\\disc.7z"));
    List l2 = { 0 }; wchar_t m2[] = L"C:\\x\0a.7z\0b.7z\0"; CHECK(split_multiselect(m2, &l2) == 2 && !strcmp(l2.v[1], "C:\\x\\b.7z"));
    CHECK(in_star(0, 0) && !in_star(.8, .8) && in_star(0, -.8));
    { path_prepend("C:\\so2_t_a"); path_prepend("C:\\so2_t_b"); char *pv = getenv_("PATH"); CHECK(pv && !strncmp(pv, "C:\\so2_t_b;C:\\so2_t_a;", 22)); }
    CHECK(!strcmp(base("C:\\a\\b.cue"), "b.cue") && !strcmp(ext_of("C:\\a.b\\c"), ""));
    fprintf(stderr, tfail ? "selftest: %d FAILED\n" : "selftest: all passed\n", tfail); return tfail ? 1 : 0;
}
