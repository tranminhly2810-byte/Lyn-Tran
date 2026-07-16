#include <obs-module.h>

#include "ultrakey-filter.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("lyn-ultrakey", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Lyn UltraKey native GPU chroma-key filter";
}

bool obs_module_load(void)
{
    obs_register_source(&lyn_ultrakey_filter_info);
    blog(LOG_INFO, "[Lyn UltraKey] module loaded");
    return true;
}
