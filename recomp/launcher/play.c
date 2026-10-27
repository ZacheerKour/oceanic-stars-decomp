/* PLAY.exe: the Steam-friendly launcher. "Add a Non-Steam Game" lists only .exe files, and Steam tracks the process it starts,
 * so this waits for the game as a direct child. play.ps1 -Prepare (hidden window) does the setup and writes play\launch.txt:
 * line 1 game exe, line 2 its arguments, line 3 run dir, then NAME=VALUE environment lines. Works from any cwd.
 * Build: recomp\launcher\build.ps1 (INSTALL.bat runs it). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static void die(const wchar_t *m) { MessageBoxW(NULL, m, L"Star Ocean: not ready", MB_OK | MB_ICONERROR); ExitProcess(1); }

static wchar_t *slurp(const wchar_t *path) {   /* UTF-8 file -> wide string (caller never frees; process is short-lived) */
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD n = GetFileSize(f, NULL), got = 0;
    char *b = malloc(n + 1);
    wchar_t *w = malloc((n + 1) * sizeof(wchar_t));
    if (!b || !w || !ReadFile(f, b, n, &got, NULL)) { CloseHandle(f); return NULL; }
    CloseHandle(f);
    w[MultiByteToWideChar(CP_UTF8, 0, b, (int)got, w, (int)n)] = 0;
    return w;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cl, int sw) {
    wchar_t dir[MAX_PATH], tmp[2 * MAX_PATH + 64], p[MAX_PATH + 64];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    *wcsrchr(dir, L'\\') = 0;
    swprintf(p, MAX_PATH + 64, L"%ls\\play\\launch.txt", dir);
    DeleteFileW(p);

    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    DWORD rc = 1;
    swprintf(tmp, 2 * MAX_PATH + 64, L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%ls\\play.ps1\" -Prepare", dir);
    if (!CreateProcessW(NULL, tmp, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, dir, &si, &pi)) die(L"Cannot start PowerShell.");
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &rc);
    if (rc) {
        swprintf(p, MAX_PATH + 64, L"%ls\\play\\prepare.err", dir);
        wchar_t *m = slurp(p);
        die(m && *m ? m : L"Setup failed. Run INSTALL.bat first (see README.md).");
    }
    swprintf(p, MAX_PATH + 64, L"%ls\\play\\launch.txt", dir);
    wchar_t *t = slurp(p);
    if (!t) die(L"Setup wrote no launch file. Run INSTALL.bat first.");

    wchar_t *exe = t, *args = wcschr(exe, L'\n'), *run;
    if (!args) die(L"Bad launch file.");
    *args++ = 0;
    run = wcschr(args, L'\n'); if (!run) die(L"Bad launch file.");
    *run++ = 0;
    wchar_t *env = wcschr(run, L'\n'); if (!env) die(L"Bad launch file.");
    *env++ = 0;
    while (*env) {   /* NAME=VALUE lines */
        wchar_t *e = wcschr(env, L'\n'); if (e) *e = 0;
        wchar_t *eq = wcschr(env, L'=');
        if (eq) { *eq = 0; SetEnvironmentVariableW(env, eq + 1); }
        if (!e) break;
        env = e + 1;
    }
    size_t n = wcslen(exe) + wcslen(args) + 8;
    wchar_t *cmd = malloc(n * sizeof(wchar_t));
    swprintf(cmd, n, L"\"%ls\" %ls", exe, args);

    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };   /* game output -> play\last.log / last.err, as play.ps1 does */
    swprintf(p, MAX_PATH + 64, L"%ls\\last.log", run);
    HANDLE out = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    swprintf(p, MAX_PATH + 64, L"%ls\\last.err", run);
    HANDLE err = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = out; si.hStdError = err; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessW(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, run, &si, &pi)) die(L"Cannot start the game exe. Run INSTALL.bat first.");
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &rc);
    return (int)rc;
}
