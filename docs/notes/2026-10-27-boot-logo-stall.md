## 2026-10-27 Boot logo jingle cut (bead star-cean-second-0ji): root cause + fix

Cause: dirty_ram_interp.c stamps each external interpreter entry with psx_overlay_resident_crc_at() (debug-server-only per_pc
occ_crc/occ_ok), which walks the 16384-entry static-match cache every call. While the retail BIOS logo/CD init runs interpreted
(~9000 external entries per frame, guest frames 401-412) that cost ~150-200 ms per frame, so the guest ran at ~8% speed and the
SPU starved (3 host underruns, ~1 s dry). Not disc IO: no disc-read span over 1 ms (HSPAN_DISC_READ), 0 KiB IO, few page faults.
Fix: CMake patch routes the call through psxrt_occ_crc (stubs/psxrt_evlog.c), which only scans when PSXRT_OCC=1.
Measure (retail BIOS, hidden window, audio dummy, PSXRT_EVLOG=1): wall ms for guest frames 392..422 (500 = real time):
OCC=1 (old behaviour) 1670-2554, fixed 500-502 (5 runs); audio pump underruns 3 -> 0, dry 998 ms -> 0.
evlog additions: T (wall ms per 30 frames), W (frames >25 ms: wall us, interp, page faults), S (spans >=1 ms), A/U/X/Y (audio
trace underruns, XA zero-runs, pump skips). PSXRT_PROF samples now name the DLL when outside the exe.
Opening movie (XA/MDEC, frames ~1541-1722): no frame over 25 ms and no U/X/Y event in the fixed runs; one-off hitches
(frame ~700-830, ~1565 with 20-30k page faults) appeared in 2 of 6 runs and did not repeat (host load suspected), not explained.
