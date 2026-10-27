/* Headless host: no window, no audio, no input device. Prints BIOS TTY output. */
#include <stdio.h>
#include "host.h"

void host_frame(uint64_t vblank) { (void)vblank; }
void host_tty(uint32_t ch) { putchar((int)(ch & 0xFF)); }
