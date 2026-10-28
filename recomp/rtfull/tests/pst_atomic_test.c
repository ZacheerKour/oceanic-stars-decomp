/* cl /nologo /I..\stubs pst_atomic_test.c ; run in an empty scratch dir. Exercises psxrt_pst_open/commit the way boot_state_save uses them. */
#include "psxrt_pst.h"
#include <string.h>
#include <direct.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
static int content_is(const char *p, const char *s) { char b[64] = {0}; FILE *f = fopen(p, "rb"); if (!f) return 0; fread(b, 1, sizeof b - 1, f); fclose(f); return strcmp(b, s) == 0; }
/* write like boot_state_save: n = bytes to "write", fail = emulate a short write / disk full (ok = 0) */
static int save(const char *path, const char *data, int fail) {
    char tmp[300]; FILE *f = psxrt_pst_open(path, tmp, sizeof tmp);
    if (!f) return 0;
    int ok = !fail && fwrite(data, 1, strlen(data), f) == strlen(data);
    if (fail) fwrite(data, 1, 3, f);   /* partial data on disk before the failure */
    return psxrt_pst_commit(f, ok, tmp, path);
}
int main(void) {
    const char *p = "slot.pst";
    CHECK(save(p, "OLDSTATE", 0) == 1 && content_is(p, "OLDSTATE") && !exists("slot.pst.tmp"));    /* normal write, no existing file */
    CHECK(save(p, "NEWSTATE", 0) == 1 && content_is(p, "NEWSTATE") && !exists("slot.pst.tmp"));    /* normal overwrite */
    CHECK(save(p, "BADWRITE", 1) == 0 && content_is(p, "NEWSTATE") && !exists("slot.pst.tmp"));    /* failed write: old survives, tmp gone */
    _mkdir("blocked.pst.tmp");                                                                     /* tmp path unopenable (a directory) */
    { FILE *f = fopen("blocked.pst", "wb"); fputs("KEEP", f); fclose(f); }
    CHECK(save("blocked.pst", "XXXX", 0) == 0 && content_is("blocked.pst", "KEEP"));
    { char t[300]; CHECK(psxrt_pst_open("a-very-long", t, 5) == NULL); }                           /* tmp name does not fit */
    printf(fails ? "pst_atomic_test FAILED\n" : "pst_atomic_test OK\n");
    return fails != 0;
}
