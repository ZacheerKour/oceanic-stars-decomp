/* Scripted pad input and audio evidence for unattended runs (hooked into main.cpp / psx_sdl_audio.cpp by CMake patches).
 *
 * PSXRT_INPUT_SCRIPT=<file>: lines "<frame> <buttons...>" where frame counts pad samples (one per guest frame) and the
 * buttons (CROSS CIRCLE SQUARE TRIANGLE START SELECT UP DOWN LEFT RIGHT L1 L2 R1 R2 L3 R3, or a raw active-low hex word
 * 0xNNNN, or "-" for none) are held on port 1 from that frame until the next line. '#' starts a comment. Lines must be in
 * frame order. The scripted buttons are ANDed with live input, so the keyboard/gamepad keeps working.
 * PSXRT_AUDIO_RMS=1: every ~5 s of device audio, log "[psxrt-audio] rms=<r> peak=<p> frames=<n>" (S16 stereo). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void sio_set_pad_state_slot(int slot, uint16_t buttons);
uint16_t sio_get_pad_buttons_slot(int slot);

int savestate_request_save(int slot);
int savestate_request_load(int slot);
void psxrt_diag_dump_now(const char *tag, unsigned frame);   /* stubs/psxrt_diag.c */
void psxrt_prof_start(void);                                  /* stubs/psxrt_prof.c */

/* one-shot actions: "<frame> DUMP <tag>" (vram_<tag>.ppm), "<frame> SAVESTATE <slot>", "<frame> LOADSTATE <slot>";
 * they keep the pad word of the line before. LOADSTATE restores the guest only: the script frame counter runs on. */
enum { ACT_PAD, ACT_DUMP, ACT_SAVE, ACT_LOAD };
#define MAX_STEPS 8192
static struct { uint32_t frame; uint16_t word; uint8_t act; char arg[24]; } s_steps[MAX_STEPS];
static int s_nsteps = -1, s_cur = -1;
static uint32_t s_frame;

static const char *const k_names[16] = { "SELECT", "L3", "R3", "START", "UP", "RIGHT", "DOWN", "LEFT",
                                         "L2", "R2", "L1", "R1", "TRIANGLE", "CIRCLE", "CROSS", "SQUARE" };

static int parse_word(char *rest, uint16_t *out) {
    uint16_t w = 0xFFFFu;
    for (char *t = strtok(rest, " \t\r\n,+"); t; t = strtok(NULL, " \t\r\n,+")) {
        if (!strcmp(t, "-")) continue;
        if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) { w &= (uint16_t)strtoul(t, NULL, 16); continue; }
        int b = 0;
        while (b < 16 && _stricmp(t, k_names[b])) b++;
        if (b == 16) return 0;
        w &= (uint16_t)~(1u << b);
    }
    *out = w;
    return 1;
}

static void load_script(void) {
    s_nsteps = 0;
    const char *path = getenv("PSXRT_INPUT_SCRIPT");
    if (!path || !*path) return;
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[psxrt-input] cannot open %s\n", path); return; }
    char line[256];
    int ln = 0;
    while (fgets(line, sizeof line, f) && s_nsteps < MAX_STEPS) {
        ln++;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *end;
        unsigned long fr = strtoul(line, &end, 10);
        if (end == line) continue;   /* blank or comment-only line */
        uint16_t w = s_nsteps ? s_steps[s_nsteps - 1].word : 0xFFFFu;
        char verb[16] = "", arg[24] = "";
        int act = ACT_PAD;
        if (sscanf(end, " %15s %23s", verb, arg) >= 1) {
            if (!_stricmp(verb, "DUMP")) act = ACT_DUMP;
            else if (!_stricmp(verb, "SAVESTATE")) act = ACT_SAVE;
            else if (!_stricmp(verb, "LOADSTATE")) act = ACT_LOAD;
        }
        if ((act == ACT_PAD && !parse_word(end, &w)) || (s_nsteps && fr < s_steps[s_nsteps - 1].frame)) {
            fprintf(stderr, "[psxrt-input] %s:%d: bad line, ignored\n", path, ln);
            continue;
        }
        s_steps[s_nsteps].frame = (uint32_t)fr;
        s_steps[s_nsteps].word = w;
        s_steps[s_nsteps].act = (uint8_t)act;
        memcpy(s_steps[s_nsteps].arg, arg, sizeof arg);
        s_nsteps++;
    }
    fclose(f);
    fprintf(stderr, "[psxrt-input] %d steps from %s\n", s_nsteps, path);
}

void psxrt_diag_frame(unsigned frame);   /* stubs/psxrt_diag.c */

void psxrt_input_script_apply(void) {
    uint32_t fr = s_frame++;
    psxrt_diag_frame(fr);
    if (s_nsteps < 0) { psxrt_prof_start(); load_script(); }
    if (!s_nsteps) return;
    int i = s_cur;
    while (i + 1 < s_nsteps && s_steps[i + 1].frame <= fr) {
        i++;
        const char *a = s_steps[i].arg;
        switch (s_steps[i].act) {
        case ACT_DUMP: psxrt_diag_dump_now(a, fr); break;
        case ACT_SAVE: fprintf(stderr, "[psxrt-input] frame %u savestate %s -> %d\n", fr, a, savestate_request_save(atoi(a))); break;
        case ACT_LOAD: fprintf(stderr, "[psxrt-input] frame %u loadstate %s -> %d\n", fr, a, savestate_request_load(atoi(a))); break;
        }
    }
    if (i != s_cur) {
        s_cur = i;
        if (s_steps[i].act == ACT_PAD) fprintf(stderr, "[psxrt-input] frame %u pad 0x%04X\n", fr, s_steps[i].word);
    }
    if (i >= 0) sio_set_pad_state_slot(0, sio_get_pad_buttons_slot(0) & s_steps[i].word);
}

void psxrt_audio_tap(const void *buf, int bytes) {
    static int on = -1;
    static double sum;
    static int peak;
    static uint64_t n, total;
    if (on < 0) { const char *e = getenv("PSXRT_AUDIO_RMS"); on = e && *e && *e != '0'; }
    if (!on) return;
    const int16_t *s = (const int16_t *)buf;
    for (int i = 0; i < bytes / 2; i++) {
        int v = s[i];
        sum += (double)v * v;
        if (abs(v) > peak) peak = abs(v);
    }
    n += (uint64_t)(bytes / 2);
    if (n >= 44100u * 2u * 5u) {
        total += n / 2;
        fprintf(stderr, "[psxrt-audio] rms=%.1f peak=%d frames=%llu total=%llu\n", sqrt(sum / (double)n), peak,
                (unsigned long long)(n / 2), (unsigned long long)total);
        sum = 0; peak = 0; n = 0;
    }
}

#ifdef PSXRT_IO_SELFTEST
int main(void) {   /* cl /DPSXRT_IO_SELFTEST psxrt_io.c && psxrt_io.exe */
    char a[] = " CROSS START ", b[] = " 0xBFFF", c[] = " -", d[] = " BOGUS";
    uint16_t w;
    int ok = parse_word(a, &w) && w == (uint16_t)~((1u << 14) | (1u << 3));
    ok = ok && parse_word(b, &w) && w == 0xBFFF && parse_word(c, &w) && w == 0xFFFF && !parse_word(d, &w);
    puts(ok ? "selftest OK" : "selftest FAILED");
    return !ok;
}
uint16_t sio_get_pad_buttons_slot(int s) { (void)s; return 0xFFFF; }
void sio_set_pad_state_slot(int s, uint16_t b) { (void)s; (void)b; }
void psxrt_diag_frame(unsigned f) { (void)f; }
void psxrt_diag_dump_now(const char *t, unsigned f) { (void)t; (void)f; }
void psxrt_prof_start(void) {}
int savestate_request_save(int s) { return s; }
int savestate_request_load(int s) { return s; }
#endif
