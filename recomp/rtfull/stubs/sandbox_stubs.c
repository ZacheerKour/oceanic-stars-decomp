/* psxrt sandbox layer: linked into psxrecomp-game.exe ahead of the system import libraries.
 *
 * Every Win32 API declared dllimport is called through a pointer named __imp_<name>. Defining that pointer here makes
 * the linker take ours and never pull the import member from kernel32/ws2_32/shell32.lib, so for all code statically
 * linked into the exe (runtime + static CRT, whose system()/_popen() also end in CreateProcessW):
 *   - process creation fails (CreateProcessA/W, ShellExecute*, WinExec),
 *   - LoadLibraryA/W fail; LoadLibraryExW only loads bare System32 names (bcrypt.dll, CRT api-ms-win-* thunks),
 *   - Winsock and iphlpapi are never imported: every socket call fails, so nothing reaches the network,
 *   - CHD images are refused (libchdr is not built).
 * SDL3 is a separate DLL and keeps its own imports. build.ps1 checks the exe's import table afterwards.
 */
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#include "libchdr/chd.h"

__declspec(dllimport) void __stdcall SetLastError(unsigned long);
__declspec(dllimport) void *__stdcall GetModuleHandleW(const wchar_t *);
__declspec(dllimport) void *__stdcall GetProcAddress(void *, const char *);

#define ERROR_ACCESS_DENIED 5
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#define FAIL(ret, what) do { fprintf(stderr, "[psxrt-sandbox] blocked %s\n", what); SetLastError(ERROR_ACCESS_DENIED); return ret; } while (0)

/* ---- process creation ---- */
static int __stdcall sb_CreateProcessA(void) { FAIL(0, "CreateProcessA"); }
static int __stdcall sb_CreateProcessW(void) { FAIL(0, "CreateProcessW"); }
static intptr_t __stdcall sb_ShellExecuteA(void) { FAIL(5, "ShellExecuteA"); }   /* <= 32 = failure */
static intptr_t __stdcall sb_ShellExecuteW(void) { FAIL(5, "ShellExecuteW"); }
static int __stdcall sb_ShellExecuteExW(void) { FAIL(0, "ShellExecuteExW"); }
static unsigned __stdcall sb_WinExec(void) { FAIL(5, "WinExec"); }
void *__imp_CreateProcessA = (void *)sb_CreateProcessA;
void *__imp_CreateProcessW = (void *)sb_CreateProcessW;
void *__imp_ShellExecuteA = (void *)sb_ShellExecuteA;
void *__imp_ShellExecuteW = (void *)sb_ShellExecuteW;
void *__imp_ShellExecuteExW = (void *)sb_ShellExecuteExW;
void *__imp_WinExec = (void *)sb_WinExec;

/* ---- DLL loading ---- */
static void *__stdcall sb_LoadLibraryA(const char *name) {
    fprintf(stderr, "[psxrt-sandbox] blocked LoadLibraryA(%s)\n", name ? name : "(null)");
    SetLastError(ERROR_ACCESS_DENIED); return 0;
}
static void *__stdcall sb_LoadLibraryW(const wchar_t *name) {
    fprintf(stderr, "[psxrt-sandbox] blocked LoadLibraryW(%ls)\n", name ? name : L"(null)");
    SetLastError(ERROR_ACCESS_DENIED); return 0;
}
static void *__stdcall sb_LoadLibraryExA(const char *name, void *file, unsigned long flags) {
    (void)file; (void)flags;
    fprintf(stderr, "[psxrt-sandbox] blocked LoadLibraryExA(%s)\n", name ? name : "(null)");
    SetLastError(ERROR_ACCESS_DENIED); return 0;
}
typedef void *(__stdcall *LoadLibraryExW_fn)(const wchar_t *, void *, unsigned long);
static void *__stdcall sb_LoadLibraryExW(const wchar_t *name, void *file, unsigned long flags) {
    /* bare name + System32-only search: system DLLs only, never a path the runtime built (overlay/BIOS module cache) */
    if (name && !file && (flags & LOAD_LIBRARY_SEARCH_SYSTEM32) && !wcschr(name, L'\\') && !wcschr(name, L'/') && !wcschr(name, L':')) {
        static LoadLibraryExW_fn real;
        if (!real) real = (LoadLibraryExW_fn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryExW");
        if (real) return real(name, 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    fprintf(stderr, "[psxrt-sandbox] blocked LoadLibraryExW(%ls, flags=0x%lx)\n", name ? name : L"(null)", flags);
    SetLastError(ERROR_ACCESS_DENIED); return 0;
}
void *__imp_LoadLibraryA = (void *)sb_LoadLibraryA;
void *__imp_LoadLibraryW = (void *)sb_LoadLibraryW;
void *__imp_LoadLibraryExA = (void *)sb_LoadLibraryExA;
void *__imp_LoadLibraryExW = (void *)sb_LoadLibraryExW;

/* ---- network: Winsock + iphlpapi never imported ---- */
#define WSAENETDOWN 10050
#define WSASYSNOTREADY 10091
#define WSAHOST_NOT_FOUND 11001
#define NET_FAIL(name, ret) \
    static intptr_t __stdcall sb_##name(void) { fprintf(stderr, "[psxrt-sandbox] blocked " #name "\n"); return (intptr_t)(ret); } \
    void *__imp_##name = (void *)sb_##name;
NET_FAIL(WSAStartup, WSASYSNOTREADY)
NET_FAIL(socket, ~(uintptr_t)0)            /* INVALID_SOCKET */
NET_FAIL(getaddrinfo, WSAHOST_NOT_FOUND)
NET_FAIL(bind, -1)
NET_FAIL(connect, -1)
NET_FAIL(listen, -1)
NET_FAIL(accept, ~(uintptr_t)0)
NET_FAIL(send, -1)
NET_FAIL(sendto, -1)
NET_FAIL(recv, -1)
NET_FAIL(recvfrom, -1)
NET_FAIL(select, -1)
NET_FAIL(setsockopt, -1)
NET_FAIL(getsockopt, -1)
NET_FAIL(getsockname, -1)
NET_FAIL(ioctlsocket, -1)
NET_FAIL(shutdown, -1)
NET_FAIL(GetAdaptersAddresses, 50)          /* ERROR_NOT_SUPPORTED */
static intptr_t __stdcall sb_inet_pton(void) { return 0; }
static intptr_t __stdcall sb_inet_ntop(void) { return 0; }
static int __stdcall sb_WSAGetLastError(void) { return WSAENETDOWN; }
static int __stdcall sb_WSACleanup(void) { return 0; }
static int __stdcall sb_closesocket(void) { return 0; }
static void __stdcall sb_freeaddrinfo(void) { }
static uint16_t __stdcall sb_htons(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static uint32_t __stdcall sb_htonl(uint32_t v) { return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24); }
void *__imp_inet_pton = (void *)sb_inet_pton;
void *__imp_inet_ntop = (void *)sb_inet_ntop;
void *__imp_WSAGetLastError = (void *)sb_WSAGetLastError;
void *__imp_WSACleanup = (void *)sb_WSACleanup;
void *__imp_closesocket = (void *)sb_closesocket;
void *__imp_freeaddrinfo = (void *)sb_freeaddrinfo;
void *__imp_htons = (void *)sb_htons;
void *__imp_ntohs = (void *)sb_htons;
void *__imp_htonl = (void *)sb_htonl;
void *__imp_ntohl = (void *)sb_htonl;

/* ---- libchdr (not built): every CHD open fails ---- */
chd_error chd_open(const char *f, int m, chd_file *p, chd_file **c) { (void)f; (void)m; (void)p; if (c) *c = 0; return CHDERR_UNSUPPORTED_FORMAT; }
void chd_close(chd_file *c) { (void)c; }
const chd_header *chd_get_header(chd_file *c) { (void)c; return 0; }
chd_error chd_read(chd_file *c, uint32_t h, void *b) { (void)c; (void)h; (void)b; return CHDERR_UNSUPPORTED_FORMAT; }
chd_error chd_get_metadata(chd_file *c, uint32_t t, uint32_t i, void *o, uint32_t ol, uint32_t *rl, uint32_t *rt, uint8_t *rf) {
    (void)c; (void)t; (void)i; (void)o; (void)ol; (void)rl; (void)rt; (void)rf; return CHDERR_METADATA_NOT_FOUND;
}
/* main.cpp reaches inet_ntop without dllimport (plain symbol), so it needs a plain definition too */
const char *inet_ntop(int af, const void *src, char *dst, size_t size) { (void)af; (void)src; (void)dst; (void)size; return 0; }
