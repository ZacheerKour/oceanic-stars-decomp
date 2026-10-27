/* Force-included into the patched main.cpp and psx_sdl_audio.cpp (stubs/psxrt_audio.c; CMake patches cannot contain ';'). */
#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_audio.h>
#ifdef __cplusplus
extern "C" {
#endif
void psxrt_audio_pulled(const void *bridge, const void *pcm, int bytes);
/* main.cpp's event pumping goes through these: the Win32 message pump runs on its own fiber, so a window move/size
 * modal loop no longer stops the emulation (and with it the SPU audio). */
void psxrt_pump_events(void);
bool psxrt_poll_event(SDL_Event *ev);
/* psx_sdl_audio.cpp (PSXRT_AUDIO_ACCESSORS): the current device stream and a reopen on the default playback device. */
SDL_AudioDeviceID psxrt_audio_device(void);
int psxrt_audio_reopen(void);
#ifdef __cplusplus
}
#endif
#define SDL_PumpEvents psxrt_pump_events
#define SDL_PollEvent psxrt_poll_event

/* Expanded inside psx_sdl_audio.cpp after its stream statics. A stream from SDL_OpenAudioDeviceStream cannot be rebound,
 * so a reopen makes a new one with the same format and callback and then drops the old one. */
#define PSXRT_AUDIO_ACCESSORS \
    static void SDLCALL psx_sdl3_audio_feed(void *, SDL_AudioStream *, int, int); \
    extern "C" SDL_AudioDeviceID psxrt_audio_device(void) { return s_audio_stream ? SDL_GetAudioStreamDevice(s_audio_stream) : 0; } \
    extern "C" int psxrt_audio_reopen(void) { \
        SDL_AudioSpec spec; \
        if (!s_audio_stream || !SDL_GetAudioStreamFormat(s_audio_stream, &spec, nullptr)) return -1; \
        SDL_AudioStream *n = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, \
                                                       s_audio_callback ? psx_sdl3_audio_feed : nullptr, nullptr); \
        if (!n) return -1; \
        SDL_AudioStream *old = s_audio_stream; \
        const bool paused = SDL_AudioStreamDevicePaused(old); \
        s_audio_stream = n; \
        SDL_DestroyAudioStream(old); \
        return paused || SDL_ResumeAudioStreamDevice(n) ? 0 : -1; \
    }
