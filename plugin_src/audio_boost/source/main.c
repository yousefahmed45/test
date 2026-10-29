/*
 * audio_boost - STAGE 1 TEST BUILD (no hooks, no audio changes).
 * It only writes /data/GoldHEN/audio_boost.log to prove the plugin loads.
 * Safe to run: it does not touch the game's audio at all.
 */
#include <Common.h>
#include "plugin_common.h"

#define LOG_PATH "/data/GoldHEN/audio_boost.log"

attr_public const char *g_pluginName = "audio_boost";
attr_public const char *g_pluginDesc = "Stage 1: load test only";
attr_public const char *g_pluginAuth = "you";
attr_public u32 g_pluginVersion = 0x00000100; // 1.00

s32 attr_public plugin_load(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    final_printf("[GoldHEN] Plugin Author(s): %s\n", g_pluginAuth);
    boot_ver();

    FILE *lf = fopen(LOG_PATH, "w");
    if (lf) {
        fprintf(lf, "audio_boost stage1: plugin_load reached, no hooks installed\n");
        fclose(lf);
    }
    return 0;
}

s32 attr_public plugin_unload(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
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
