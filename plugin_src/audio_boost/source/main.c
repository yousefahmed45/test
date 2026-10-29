/*
 * audio_boost - GoldHEN plugin that boosts game audio output.   (DIAGNOSTIC BUILD)
 *
 * Hooks sceAudioOutOpen (learn each port's format) and sceAudioOutOutput
 * (scale the samples). Only the MAIN game audio port is boosted.
 *
 * Config:  /data/GoldHEN/audio_boost.ini   gain=200   (percent, 100 = untouched)
 * Log:     /data/GoldHEN/audio_boost.log   (rewritten on every game launch;
 *          open it over FTP to see what the plugin did)
 *
 * Structure follows plugin_src/fliprate_remover and the SDK's Utilities.h:
 * HOOK32(name) takes the address of the real function, so the plugin links
 * against libSceAudioOut (-lSceAudioOut in the Makefile).
 */
#include <Common.h>
#include "plugin_common.h"
#include "dsp.h"

#define AUDIO_OUT_PORT_MAIN   0
#define DEFAULT_GAIN_PERCENT  200
#define CONFIG_PATH           "/data/GoldHEN/audio_boost.ini"
#define LOG_PATH              "/data/GoldHEN/audio_boost.log"
#define OUTPUT_LOG_EVERY      4000   /* log a line every N output calls */

attr_public const char *g_pluginName = "audio_boost";
attr_public const char *g_pluginDesc = "Boosts main game audio output";
attr_public const char *g_pluginAuth = "you";
attr_public u32 g_pluginVersion = 0x00000100; // 1.00

static volatile s32 g_gain_q8 = (DEFAULT_GAIN_PERCENT * 256) / 100;
static volatile u32 g_out_calls = 0;

/* File log: open/append/close each time so nothing is lost if the game crashes.
 * Never called on every audio frame, only on open and every OUTPUT_LOG_EVERY frames. */
#define FLOG(...) do { \
    FILE *lf_ = fopen(LOG_PATH, "a"); \
    if (lf_) { fprintf(lf_, __VA_ARGS__); fclose(lf_); } \
} while (0)

/* HOOK32(name) needs the real function name to be declared. */
s32 sceAudioOutOpen(s32 userId, s32 type, s32 index, u32 len, u32 freq, u32 param);
s32 sceAudioOutOutput(s32 handle, void *ptr);

HOOK_INIT(sceAudioOutOpen);
HOOK_INIT(sceAudioOutOutput);

static void load_config(void)
{
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) { FLOG("config: %s not found, default gain used\n", CONFIG_PATH); return; }
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

    FLOG("open: handle=%d type=%d index=%d len=%u freq=%u param=0x%x\n",
         handle, type, index, len, freq, param);

    if (handle > 0) {
        if (type == AUDIO_OUT_PORT_MAIN)
            ab_register(handle, len, param);
        else
            ab_forget(handle);
    }
    return handle;
}

s32 sceAudioOutOutput_hook(s32 handle, void *buf)
{
    u32 n = ++g_out_calls;
    if (n == 1 || (n % OUTPUT_LOG_EVERY) == 0)
        FLOG("output: call #%u handle=%d buf=%p gain_q8=%d boosted=%d\n",
             n, handle, buf, (int)g_gain_q8, ab_find(handle) != NULL);

    ab_apply(handle, buf, g_gain_q8);   /* no-op for NULL buf / unknown ports / gain 100 */
    return HOOK_CONTINUE(sceAudioOutOutput, s32 (*)(s32, void *), handle, buf);
}

s32 attr_public plugin_load(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    final_printf("[GoldHEN] Plugin Author(s): %s\n", g_pluginAuth);
    boot_ver();

    /* start a fresh log for this launch */
    FILE *lf = fopen(LOG_PATH, "w");
    if (lf) { fprintf(lf, "audio_boost: plugin_load reached\n"); fclose(lf); }

    load_config();
    FLOG("gain_q8 = %d (256 = 1.0x)\n", (int)g_gain_q8);

    HOOK32(sceAudioOutOpen);
    HOOK32(sceAudioOutOutput);

    FLOG("hooks installed\n");
    return 0;
}

s32 attr_public plugin_unload(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    UNHOOK(sceAudioOutOpen);
    UNHOOK(sceAudioOutOutput);
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
