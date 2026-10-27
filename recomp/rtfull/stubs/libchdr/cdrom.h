/* Stand-in for libchdr's cdrom.h (constants iso_reader.cpp uses; values as in libchdr). */
#ifndef PSXRT_STUB_CHD_CDROM_H
#define PSXRT_STUB_CHD_CDROM_H
#include "chd.h"
#define CD_MAX_TRACKS 99
#define CD_MAX_SECTOR_DATA 2352
#define CD_MAX_SUBCODE_DATA 96
#define CD_FRAME_SIZE (CD_MAX_SECTOR_DATA + CD_MAX_SUBCODE_DATA)
#define CD_TRACK_PADDING 4
#define CDROM_TRACK_METADATA_TAG CHD_MAKE_TAG('C','H','T','R')
#define CDROM_TRACK_METADATA2_TAG CHD_MAKE_TAG('C','H','T','2')
#endif
