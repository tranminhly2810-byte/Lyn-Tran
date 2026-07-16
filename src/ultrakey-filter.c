#include "ultrakey-filter.h"

struct lyn_ultrakey_filter {
    obs_source_t *context;
};

static const char *lyn_ultrakey_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("LynUltraKey.FilterName");
}

static void *lyn_ultrakey_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);

    struct lyn_ultrakey_filter *filter =
        bzalloc(sizeof(struct lyn_ultrakey_filter));

    filter->context = source;

    blog(LOG_INFO, "[Lyn UltraKey] filter instance created");
    return filter;
}

static void lyn_ultrakey_destroy(void *data)
{
    struct lyn_ultrakey_filter *filter = data;

    blog(LOG_INFO, "[Lyn UltraKey] filter instance destroyed");
    bfree(filter);
}

static void lyn_ultrakey_update(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(settings);
}

static void lyn_ultrakey_get_defaults(obs_data_t *settings)
{
    UNUSED_PARAMETER(settings);
}

static obs_properties_t *lyn_ultrakey_get_properties(void *data)
{
    UNUSED_PARAMETER(data);

    obs_properties_t *props = obs_properties_create();

    obs_properties_add_text(
        props,
        "milestone_status",
        obs_module_text("LynUltraKey.Status"),
        OBS_TEXT_INFO
    );

    return props;
}

static void lyn_ultrakey_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);

    struct lyn_ultrakey_filter *filter = data;

    if (!filter || !filter->context) {
        return;
    }

    /*
     * Milestone 1 is intentionally pass-through.
     * The filter appears in OBS without changing the image.
     * Milestone 2 will replace this with the UltraKey GPU effect.
     */
    obs_source_skip_video_filter(filter->context);
}

struct obs_source_info lyn_ultrakey_filter_info = {
    .id = "lyn_ultrakey_filter",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_VIDEO,
    .get_name = lyn_ultrakey_get_name,
    .create = lyn_ultrakey_create,
    .destroy = lyn_ultrakey_destroy,
    .update = lyn_ultrakey_update,
    .get_defaults = lyn_ultrakey_get_defaults,
    .get_properties = lyn_ultrakey_get_properties,
    .video_render = lyn_ultrakey_video_render,
};
