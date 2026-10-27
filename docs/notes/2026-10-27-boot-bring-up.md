## 2026-10-27 boot bring-up
Runs: `recomp\rt\run_rt.ps1 -Name <n> -Roms <home>\star-cean-second\roms -Vblanks N -Frames 60` (logs, `ram.bin`, `vram.ppm`, `display.ppm`, `frames\fNNNNN.ppm` in the gitignored `recomp\rt\_out\`).
Debug switches added to `rt_boot.exe`: `--trace-pc <hex>` (registers when the interpreter reaches a pc, 40 hits), `--watch <hex>` (stores to one word, 60 hits), `--dump <dir>`, `--frames N`.
The run now writes files, but only into the `--dump` directory (`_out`).

1. **Exception-vector clobber at 0x8003F900 = missing GPU read-back.** The function at 0x8003F2xx (program A, logo "dissolve" effect) mallocs a buffer, `StoreImage`s the
   framebuffer into it (0x80021168, DMA2 GPU->RAM), `DrawSync`s, then adds one particle per non-zero pixel to `[obj+0x2C]`. rt's DMA2 did nothing, so the buffer kept heap garbage:
   `--watch 800EE564` showed the count climbing to 0x111C (4380) at vblank 271; the primitive-pool allocator 0x8003C434 (pool 0, 2 x 64 KB at 0x80107578/0x80117588) then
   failed (`--trace-pc 8003c484`: `a1 000155CC` = 4380*20 bytes) and returned NULL, and 0x8003F7B8's caller filled packets at NULL+0x10.. over 0x80-0x8C.
   Not a generated-code fault. **Fix:** `recomp/rt/gpu.c`, a software GPU (1 MB VRAM, GP0 fill/copy/CPU<->VRAM/DMA2 block both directions, linked lists fed to GP0,
   flat/gouraud/textured polygons, rectangles, lines, draw area/offset, GPUSTAT from GP1 state). Evidence: run `g1`, 600 vblanks, no vector write, `gpu cmds 107577`;
   `vram.ppm` shows the tri-Ace / ENIX / SCEA logos and the dissolve particles. Commit 1129028.
2. **`VSync: timeout` x200 = cycle model too fast for compiled loops.** Vcount (0x8002D024) did advance (0x10B at stop). libetc `v_wait` (0x8001FFC8) counts `timeout << 15`
   iterations of a 2-block loop; rt charged 8 cycles per compiled block, so 32768 iterations took ~524k cycles < one frame (564,480). **Fix:** 15 cycles per block
   (`psx_check_interrupts_at`). Evidence: run `g2`, 1200 vblanks, 0 `VSync: timeout` (was 462 in 600), C0.0A ChangeClearRCnt calls 464 -> 2.
3. **Stall in program A at 0x800504B4 (MDEC busy wait) = no MDEC.** After the logos the game resets the MDEC (libpress) and polls MDEC1 status bit 29 (busy); rt returned
   the last written control word (0x60000000), so it waited forever (run `g2`, unmodelled I/O `1F801824/0`, ReadS at LBA 138051). **Fix:** `recomp/rt/mdec.c` (run-length +
   float IDCT + YUV->RGB, 24/15-bit output, quant tables; DMA0 in, DMA1 out deferred until the decoder has the words) and 24-bit display dumps.
   Evidence: run `m1`, 2400 vblanks, `dma: ch0=425 ch1=8420`, 4635 sectors read; frames show the LINKS logo and the opening movie (planets).
4. **Title menu reached.** Run `m2` (12,000 vblanks, `-Frames 150`, ~25 min wall clock): no stop before the vblank cap, 0 `VSync: timeout`, 27,025 sectors read,
   `dma: ch0=2649 ch1=52900`. Frames: opening movie with credits (f06150-f10350), Star Ocean logo, then the 640x480 title menu (Star Ocean logo, "New Game / Continue /
   Voice Collection") first seen in `f11400` (not in `f11250`; dumps are 150 vblanks apart, so it appeared within that window). Program A still runs fully interpreted
   (1.82 G interpreted instructions in 12,000 vblanks; hottest page RAM+0x50800).
   **Next blocker:** no controller. SIO0 reads return 0xFF / status 7 (rt.c `io_read`, "no controller"), so the BIOS pad buffers never get a pad and the menu cannot be
   driven past "New Game". Needs a SIO0 digital-pad model (0x01 0x42 handshake, ID 0x41 0x5A, two button bytes, /ACK IRQ7) fed from the host input seam, and then the
   field/battle programs B/C (not yet generated as compiled code; they will interpret). Audio (SPU) is still a stub.
