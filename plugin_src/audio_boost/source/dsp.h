/*
 * dsp.h - audio boost logic for sceAudioOutOutput hook.
 * No PS4 / GoldHEN SDK dependencies, so it can be unit-tested on a PC.
 *
 * Gain is fixed point, Q8: 256 = 1.0x, 512 = 2.0x, 1024 = 4.0x.
 */
#ifndef AUDIO_BOOST_DSP_H
#define AUDIO_BOOST_DSP_H

#include <stdint.h>
#include <stddef.h>

#define AB_MAX_PORTS   16
#define AB_GAIN_UNITY  256
#define AB_GAIN_MIN    256     /* never attenuate */
#define AB_GAIN_MAX    2048    /* 8x ceiling, keeps int math safe */

typedef struct {
    int32_t  handle;        /* 0 = free slot */
    uint32_t sample_count;  /* grain length * channels */
    uint8_t  is_float;
} ab_port_t;

static ab_port_t g_ab_ports[AB_MAX_PORTS];

/* Decode the format bits of sceAudioOutOpen's `param` argument.
 * 0 S16_MONO  1 S16_STEREO  2 S16_8CH  3 FLOAT_MONO
 * 4 FLOAT_STEREO  5 FLOAT_8CH  6 S16_8CH_STD  7 FLOAT_8CH_STD */
static int ab_decode_format(uint32_t param, uint32_t *channels, int *is_float)
{
    switch (param & 0xFFu) {
    case 0: *channels = 1; *is_float = 0; return 1;
    case 1: *channels = 2; *is_float = 0; return 1;
    case 2: *channels = 8; *is_float = 0; return 1;
    case 3: *channels = 1; *is_float = 1; return 1;
    case 4: *channels = 2; *is_float = 1; return 1;
    case 5: *channels = 8; *is_float = 1; return 1;
    case 6: *channels = 8; *is_float = 0; return 1;
    case 7: *channels = 8; *is_float = 1; return 1;
    default: return 0;
    }
}

static int32_t ab_clamp_gain(int32_t g)
{
    if (g < AB_GAIN_MIN) return AB_GAIN_MIN;
    if (g > AB_GAIN_MAX) return AB_GAIN_MAX;
    return g;
}

static ab_port_t *ab_find(int32_t handle)
{
    for (int i = 0; i < AB_MAX_PORTS; i++)
        if (g_ab_ports[i].handle == handle)
            return &g_ab_ports[i];
    return NULL;
}

/* Remember a port's format. Unknown formats are NOT registered, so their
 * audio is passed through untouched (safer than guessing and making noise). */
static void ab_register(int32_t handle, uint32_t grain_len, uint32_t param)
{
    if (handle <= 0) return;

    uint32_t ch = 0;
    int fl = 0;
    ab_port_t *p = ab_find(handle);

    if (!ab_decode_format(param, &ch, &fl)) {
        if (p) p->handle = 0;           /* forget a stale entry */
        return;
    }
    if (!p) {
        for (int i = 0; i < AB_MAX_PORTS; i++)
            if (g_ab_ports[i].handle == 0) { p = &g_ab_ports[i]; break; }
    }
    if (!p) return;                      /* table full: leave port unboosted */

    p->handle       = handle;
    p->sample_count = grain_len * ch;
    p->is_float     = (uint8_t)fl;
}

static void ab_forget(int32_t handle)
{
    ab_port_t *p = ab_find(handle);
    if (p) p->handle = 0;
}

/* Boost one grain in place. Returns 1 if the buffer was modified. */
static int ab_apply(int32_t handle, void *buf, int32_t gain_q8)
{
    if (!buf) return 0;                  /* NULL buffer = wait/flush call */

    ab_port_t *p = ab_find(handle);
    if (!p) return 0;

    gain_q8 = ab_clamp_gain(gain_q8);
    if (gain_q8 == AB_GAIN_UNITY) return 0;

    if (p->is_float) {
        float *s = (float *)buf;
        const float g = (float)gain_q8 / 256.0f;
        for (uint32_t i = 0; i < p->sample_count; i++) {
            float v = s[i] * g;
            if (v > 1.0f)       v = 1.0f;
            else if (v < -1.0f) v = -1.0f;
            s[i] = v;
        }
    } else {
        int16_t *s = (int16_t *)buf;
        for (uint32_t i = 0; i < p->sample_count; i++) {
            int32_t v = ((int32_t)s[i] * gain_q8) / 256;
            if (v > 32767)       v = 32767;
            else if (v < -32768) v = -32768;
            s[i] = (int16_t)v;
        }
    }
    return 1;
}

#endif /* AUDIO_BOOST_DSP_H */
