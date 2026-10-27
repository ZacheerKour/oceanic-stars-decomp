/* Stand-in for libchdr's chd.h: CHD images are not supported in this build (we read .cue/.bin).
   Only the names iso_reader.cpp uses; every call fails (sandbox_stubs.c). */
#ifndef PSXRT_STUB_CHD_H
#define PSXRT_STUB_CHD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CHDERR_NONE = 0, CHDERR_FILE_NOT_FOUND = 3, CHDERR_UNSUPPORTED_FORMAT = 11,
               CHDERR_METADATA_NOT_FOUND = 16 } chd_error;
typedef struct chd_file chd_file;
typedef struct { uint32_t length, version, flags, hunkbytes, totalhunks; uint64_t logicalbytes; } chd_header;
#define CHD_OPEN_READ 1
#define CHD_MAKE_TAG(a,b,c,d) ((((uint32_t)(a)) << 24) | (((uint32_t)(b)) << 16) | (((uint32_t)(c)) << 8) | ((uint32_t)(d)))
chd_error chd_open(const char *filename, int mode, chd_file *parent, chd_file **chd);
void chd_close(chd_file *chd);
const chd_header *chd_get_header(chd_file *chd);
chd_error chd_read(chd_file *chd, uint32_t hunknum, void *buffer);
chd_error chd_get_metadata(chd_file *chd, uint32_t searchtag, uint32_t searchindex, void *output,
                           uint32_t outputlen, uint32_t *resultlen, uint32_t *resulttag, uint8_t *resultflags);
#ifdef __cplusplus
}
#endif
#endif
