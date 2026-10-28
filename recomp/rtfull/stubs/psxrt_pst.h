/* Atomic .pst publish (psxrt_patch on boot_state.c, force-included). The runtime opened the slot with fopen "wb" (truncating the
 * last good state), deleted it on a failed write and ignored fclose, so a crash or full disk during F5 lost the old state and a
 * short write could still toast "Saved". Same scheme as the runtime's memcard publish_card(): write <slot>.tmp, flush, commit
 * to disk, check fclose, then MoveFileEx over the slot. On any failure the old slot is untouched and only the tmp is removed. */
#ifndef PSXRT_PST_H
#define PSXRT_PST_H
#include <stdio.h>
#include <io.h>
#include <windows.h>

static FILE *psxrt_pst_open(const char *path, char *tmp, size_t cap) {
    if (snprintf(tmp, cap, "%s.tmp", path) >= (int)cap) return NULL;
    return fopen(tmp, "wb");
}

/* f: stream from psxrt_pst_open; ok: whether every write succeeded. Closes f. Returns 1 only when the slot now holds the new state. */
static int psxrt_pst_commit(FILE *f, int ok, const char *tmp, const char *path) {
    if (fflush(f) != 0) ok = 0;
    if (ok && _commit(_fileno(f)) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (ok) ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) remove(tmp);
    return ok;
}
#endif
