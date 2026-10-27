/* Thin host interface. Headless today (host_headless.c); an SDL3 build would
 * implement the same functions once SDL3 is supplied and audited. */
#ifndef RT_HOST_H
#define RT_HOST_H
#include <stdint.h>

void host_frame(uint64_t vblank);   /* once per emulated vblank (present/poll later) */
void host_tty(uint32_t ch);         /* BIOS putchar output */

/* CD-ROM controller (cdrom.c): register file 0x1F801800-03, DMA3, disc image read-only. */
void     host_cd_open(const char *bin_path);
uint32_t host_cd_read(uint32_t reg);
void     host_cd_write(uint32_t reg, uint8_t v, uint32_t *i_stat);
void     host_cd_dma(uint8_t *ram, uint32_t addr, uint32_t words);
void     host_cd_tick(uint64_t cycles, uint32_t *i_stat);
void     host_cd_summary(void);

/* Software GPU (gpu.c): VRAM, GP0/GP1, transfers, rasteriser, PPM dumps. */
void     gpu_gp0(uint32_t w);
void     gpu_gp1(uint32_t w);
uint32_t gpu_read(void);
uint32_t gpu_stat(int odd_field);
void     gpu_dump_vram(const char *path);
void     gpu_dump_display(const char *path);
extern uint64_t g_gpu_prims;

/* MDEC (mdec.c): 0x1F801820 (MDEC0) / 0x1F801824 (MDEC1), DMA0 in, DMA1 out. */
void     mdec_write0(uint32_t v);
void     mdec_write1(uint32_t v);
uint32_t mdec_read0(void);
uint32_t mdec_status(void);
uint32_t mdec_out_avail(void);
#endif
