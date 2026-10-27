/* Force-included into the patched main.cpp and psx_sdl_audio.cpp (CMake patches cannot contain ';'). */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void psxrt_input_script_apply(void);
void psxrt_audio_tap(const void *buf, int bytes);
#ifdef __cplusplus
}
#endif
