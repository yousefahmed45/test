/*
 * audio_boost - GoldHEN plugin that boosts game audio output.
 *
 * Hooks:
 *   sceAudioOutOpen   -> remember each port's format (S16/float, channels, grain)
 *   sceAudioOutOutput -> multiply the samples before the game's audio is played
 *   sceAudioOutClose  -> forget the port
 *
 * Only the MAIN game audio port is boosted (voice chat / system ports untouched).
 * Optional config: /data/GoldHEN/audio_boost.ini containing  gain=200
 * (percent: 100 = normal, 200 = 2x, 300 = 3x, max 800). Default is 200.
 *
 * !! The audio math lives in dsp.h and is tested. The parts that touch the
 * !! GoldHEN SDK are marked "SDK GLUE" below. They were written from memory
 * !! of the SDK, so if the compiler complains, copy the exact macro / entry
 * !! point names from plugin_src/fliprate_remover and plugin_template in
 * !! GoldHEN_Plugins_Repository. Nothing else needs to change.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/libkernel.h>

#include "plugin_common.h"   /* SDK GLUE: attr_public, final_printf, boot_ver, HOOK macros */
#include "dsp.h"

#define AUDIO_OUT_PORT_MAIN   0
#define DEFAULT_GAIN_PERCENT  200
#define CONFIG_PATH           "/data/GoldHEN/audio_boost.ini"

/* SDK GLUE: plugin metadata shown in the klog */
attr_public const char *g_pluginName = "audio_boost";
attr_public const char *g_pluginDesc = "Boosts main game audio output";
attr_public const char *g_pluginAuth = "you";
attr_public uint32_t    g_pluginVersion = 0x00000100; /* 1.00 */

static volatile int32_t g_gain_q8 = (DEFAULT_GAIN_PERCENT * 256) / 100;

/* SDK GLUE: one HOOK_INIT per hooked function */
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

static int32_t sceAudioOutOpen_hook(int32_t userId, int32_t type, int32_t index,
                                    uint32_t len, uint32_t freq, uint32_t param)
{
    /* SDK GLUE: call the original function */
    int32_t handle = HOOK_CONTINUE(sceAudioOutOpen,
        int32_t (*)(int32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t),
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

static int32_t sceAudioOutOutput_hook(int32_t handle, void *buf)
{
    ab_apply(handle, buf, g_gain_q8);   /* no-op for NULL buf / unknown ports */

    /* SDK GLUE: call the original function */
    return HOOK_CONTINUE(sceAudioOutOutput,
        int32_t (*)(int32_t, void *), handle, buf);
}

static int32_t sceAudioOutClose_hook(int32_t handle)
{
    ab_forget(handle);
    return HOOK_CONTINUE(sceAudioOutClose, int32_t (*)(int32_t), handle);
}

static void *resolve(SceKernelModule mod, const char *name)
{
    void *addr = NULL;
    if (sceKernelDlsym(mod, name, &addr) != 0 || !addr) {
        final_printf("[audio_boost] could not resolve %s\n", name);
        return NULL;
    }
    return addr;
}

/* SDK GLUE: plugin entry point (GoldHEN's loader calls plugin_load) */
int32_t attr_public plugin_load(int32_t argc, const char *argv[])
{
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    final_printf("[GoldHEN] Plugin Author(s): %s\n", g_pluginAuth);
    boot_ver();

    load_config();
    final_printf("[audio_boost] gain = %d/256\n", (int)g_gain_q8);

    SceKernelModule mod = sceKernelLoadStartModule("libSceAudioOut.sprx", 0, NULL, 0, NULL, NULL);
    if (mod <= 0) {
        final_printf("[audio_boost] could not load libSceAudioOut (0x%x)\n", (unsigned)mod);
        return 0;
    }

    void *a_open   = resolve(mod, "sceAudioOutOpen");
    void *a_output = resolve(mod, "sceAudioOutOutput");
    void *a_close  = resolve(mod, "sceAudioOutClose");
    if (!a_open || !a_output || !a_close) return 0;

    /* SDK GLUE: attach the hooks (address, replacement function) */
    HOOK(sceAudioOutOpen,   a_open);
    HOOK(sceAudioOutOutput, a_output);
    HOOK(sceAudioOutClose,  a_close);

    final_printf("[audio_boost] hooks installed\n");
    return 0;
}

/* SDK GLUE: plugin exit point */
int32_t attr_public plugin_unload(int32_t argc, const char *argv[])
{
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    UNHOOK(sceAudioOutOpen);
    UNHOOK(sceAudioOutOutput);
    UNHOOK(sceAudioOutClose);
    return 0;
}
