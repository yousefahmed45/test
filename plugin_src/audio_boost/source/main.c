/*
 * audio_boost - GoldHEN plugin that boosts game audio output.
 *
 * Hooks:
 *   sceAudioOutOpen   -> remember each port's format (S16/float, channels, grain)
 *   sceAudioOutOutput -> multiply the samples before the audio is played
 *   sceAudioOutClose  -> forget the port
 *
 * Only the MAIN game audio port is boosted (voice chat / system ports untouched).
 * Optional config: /data/GoldHEN/audio_boost.ini containing  gain=200
 * (percent: 100 = normal, 200 = 2x, 300 = 3x, max 800). Default is 200.
 *
 * Structure follows plugin_src/fliprate_remover (HOOK_INIT / HOOK32 / UNHOOK).
 * The audio math lives in dsp.h.
 */
#include <Common.h>
#include <orbis/libkernel.h>
#include "plugin_common.h"
#include "dsp.h"

#define AUDIO_OUT_PORT_MAIN   0
#define DEFAULT_GAIN_PERCENT  200
#define CONFIG_PATH           "/data/GoldHEN/audio_boost.ini"

attr_public const char *g_pluginName = "audio_boost";
attr_public const char *g_pluginDesc = "Boosts main game audio output";
attr_public const char *g_pluginAuth = "you";
attr_public u32 g_pluginVersion = 0x00000100; // 1.00

static volatile s32 g_gain_q8 = (DEFAULT_GAIN_PERCENT * 256) / 100;

/* HOOK32(name) needs the real function name to be declared (this is why
 * fliprate_remover includes <orbis/VideoOut.h>). These prototypes match the
 * hook functions below, so no extra SDK header is needed. */
s32 sceAudioOutOpen(s32 userId, s32 type, s32 index, u32 len, u32 freq, u32 param);
s32 sceAudioOutOutput(s32 handle, void *ptr);
s32 sceAudioOutClose(s32 handle);

HOOK_INIT(sceAudioOutOpen);
HOOK_INIT(sceAudioOutOutput);
HOOK_INIT(sceAudioOutClose);

static void load_config(void)
{
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "gain=", 5) == 0) {
            int pct = atoi(line + 5);
            if (pct > 0)
                g_gain_q8 = ab_clamp_gain((pct * 256) / 100);
        }
    }
    fclose(f);
}

s32 sceAudioOutOpen_hook(s32 userId, s32 type, s32 index, u32 len, u32 freq, u32 param)
{
    s32 handle = HOOK_CONTINUE(sceAudioOutOpen,
        s32 (*)(s32, s32, s32, u32, u32, u32),
        userId, type, index, len, freq, param);

    if (handle > 0) {
        if (type == AUDIO_OUT_PORT_MAIN)
            ab_register(handle, len, param);
        else
            ab_forget(handle);
        final_printf("[audio_boost] open handle=%d type=%d len=%u param=0x%x\n",
                     handle, type, len, param);
    }
    return handle;
}

s32 sceAudioOutOutput_hook(s32 handle, void *buf)
{
    ab_apply(handle, buf, g_gain_q8);   /* no-op for NULL buf / unknown ports */
    return HOOK_CONTINUE(sceAudioOutOutput, s32 (*)(s32, void *), handle, buf);
}

s32 sceAudioOutClose_hook(s32 handle)
{
    ab_forget(handle);
    return HOOK_CONTINUE(sceAudioOutClose, s32 (*)(s32), handle);
}

s32 attr_public plugin_load(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    final_printf("[GoldHEN] Plugin Author(s): %s\n", g_pluginAuth);
    boot_ver();

    load_config();
    final_printf("[audio_boost] gain = %d/256\n", (int)g_gain_q8);

    /* Make sure the audio library is loaded so the hooks can find it.
     * Harmless if the game already loaded it. */
    s32 mod = sceKernelLoadStartModule("/system/common/lib/libSceAudioOut.sprx",
                                       0, NULL, 0, NULL, NULL);
    final_printf("[audio_boost] libSceAudioOut load result: 0x%x\n", (unsigned)mod);

    HOOK32(sceAudioOutOpen);
    HOOK32(sceAudioOutOutput);
    HOOK32(sceAudioOutClose);

    final_printf("[audio_boost] hooks installed\n");
    return 0;
}

s32 attr_public plugin_unload(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    UNHOOK(sceAudioOutOpen);
    UNHOOK(sceAudioOutOutput);
    UNHOOK(sceAudioOutClose);
    return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args)
{
    return 0;
}

s32 attr_module_hidden module_stop(s64 argc, const void *args)
{
    return 0;
}
