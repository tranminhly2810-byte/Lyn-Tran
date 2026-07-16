#include "ultrakey-filter.h"

#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#define S_KEY_COLOR "key_color"
#define S_SIMILARITY "similarity"
#define S_SOFTNESS "softness"
#define S_EDGE_CLEAN "edge_clean"
#define S_NOISE_REJECT "noise_reject"
#define S_HAIR_DETAIL "hair_detail"
#define S_DESPILL "despill"
#define S_DESPILL_INSIDE "despill_inside"
#define S_DECONTAMINATE "edge_decontaminate"
#define S_BLACK_PROTECT "black_protect"
#define S_DEBUG_VIEW "debug_view"

struct lyn_ultrakey_filter {
	obs_source_t *context;
	gs_effect_t *effect;
	bool effect_loaded;

	gs_eparam_t *key_color_param;
	gs_eparam_t *pixel_size_param;
	gs_eparam_t *similarity_param;
	gs_eparam_t *softness_param;
	gs_eparam_t *edge_clean_param;
	gs_eparam_t *noise_reject_param;
	gs_eparam_t *hair_detail_param;
	gs_eparam_t *despill_param;
	gs_eparam_t *despill_inside_param;
	gs_eparam_t *decontaminate_param;
	gs_eparam_t *black_protect_param;
	gs_eparam_t *debug_view_param;

	struct vec4 key_color;
	float similarity;
	float softness;
	float edge_clean;
	float noise_reject;
	float hair_detail;
	float despill;
	float despill_inside;
	float edge_decontaminate;
	float black_protect;
	float debug_view;
};

static const char *lyn_ultrakey_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("LynUltraKey.FilterName");
}

static void lyn_ultrakey_cache_params(struct lyn_ultrakey_filter *filter)
{
	filter->key_color_param = gs_effect_get_param_by_name(filter->effect, "key_color");
	filter->pixel_size_param = gs_effect_get_param_by_name(filter->effect, "pixel_size");
	filter->similarity_param = gs_effect_get_param_by_name(filter->effect, "similarity");
	filter->softness_param = gs_effect_get_param_by_name(filter->effect, "softness");
	filter->edge_clean_param = gs_effect_get_param_by_name(filter->effect, "edge_clean");
	filter->noise_reject_param = gs_effect_get_param_by_name(filter->effect, "noise_reject");
	filter->hair_detail_param = gs_effect_get_param_by_name(filter->effect, "hair_detail");
	filter->despill_param = gs_effect_get_param_by_name(filter->effect, "despill");
	filter->despill_inside_param = gs_effect_get_param_by_name(filter->effect, "despill_inside");
	filter->decontaminate_param = gs_effect_get_param_by_name(filter->effect, "edge_decontaminate");
	filter->black_protect_param = gs_effect_get_param_by_name(filter->effect, "black_protect");
	filter->debug_view_param = gs_effect_get_param_by_name(filter->effect, "debug_view");
}

static bool lyn_ultrakey_load_effect(struct lyn_ultrakey_filter *filter)
{
	char *effect_path = obs_module_file("ultrakey.effect");

	if (!effect_path) {
		blog(LOG_ERROR, "[Lyn UltraKey] obs_module_file returned NULL for ultrakey.effect");
		return false;
	}

	blog(LOG_INFO, "[Lyn UltraKey] loading effect: %s", effect_path);

	char *errors = NULL;

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, &errors);

	if (filter->effect)
		lyn_ultrakey_cache_params(filter);

	obs_leave_graphics();

	if (errors) {
		blog(LOG_ERROR, "[Lyn UltraKey] shader compiler: %s", errors);
		bfree(errors);
	}

	bfree(effect_path);

	if (!filter->effect) {
		blog(LOG_ERROR, "[Lyn UltraKey] effect load failed; filter will run pass-through");
		return false;
	}

	blog(LOG_INFO, "[Lyn UltraKey] effect loaded successfully");
	return true;
}

static void lyn_ultrakey_update(void *data, obs_data_t *settings)
{
	struct lyn_ultrakey_filter *filter = data;

	if (!filter)
		return;

	const uint32_t color = (uint32_t)obs_data_get_int(settings, S_KEY_COLOR);

	vec4_from_rgba(&filter->key_color, color | 0xFF000000);

	filter->similarity = (float)obs_data_get_int(settings, S_SIMILARITY) / 1000.0f;
	filter->softness = (float)obs_data_get_int(settings, S_SOFTNESS) / 1000.0f;
	filter->edge_clean = (float)obs_data_get_int(settings, S_EDGE_CLEAN) / 1000.0f;
	filter->noise_reject = (float)obs_data_get_int(settings, S_NOISE_REJECT) / 1000.0f;
	filter->hair_detail = (float)obs_data_get_int(settings, S_HAIR_DETAIL) / 1000.0f;
	filter->despill = (float)obs_data_get_int(settings, S_DESPILL) / 1000.0f;
	filter->despill_inside = (float)obs_data_get_int(settings, S_DESPILL_INSIDE) / 1000.0f;
	filter->edge_decontaminate = (float)obs_data_get_int(settings, S_DECONTAMINATE) / 1000.0f;
	filter->black_protect = (float)obs_data_get_int(settings, S_BLACK_PROTECT) / 1000.0f;
	filter->debug_view = (float)obs_data_get_int(settings, S_DEBUG_VIEW);
}

static void *lyn_ultrakey_create(obs_data_t *settings, obs_source_t *source)
{
	struct lyn_ultrakey_filter *filter = bzalloc(sizeof(struct lyn_ultrakey_filter));

	filter->context = source;

	/*
     * Important: never return NULL merely because the shader failed.
     * Returning a valid instance keeps the properties UI available and
     * lets OBS run this filter in pass-through mode for easier diagnosis.
     */
	filter->effect_loaded = lyn_ultrakey_load_effect(filter);

	lyn_ultrakey_update(filter, settings);

	blog(LOG_INFO, "[Lyn UltraKey] instance created (effect_loaded=%s)", filter->effect_loaded ? "true" : "false");

	return filter;
}

static void lyn_ultrakey_destroy(void *data)
{
	struct lyn_ultrakey_filter *filter = data;

	if (!filter)
		return;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(filter);
}

static void lyn_ultrakey_set_float(gs_eparam_t *param, float value)
{
	if (param)
		gs_effect_set_float(param, value);
}

static void lyn_ultrakey_video_render(void *data, gs_effect_t *unused_effect)
{
	UNUSED_PARAMETER(unused_effect);

	struct lyn_ultrakey_filter *filter = data;

	if (!filter || !filter->context || !filter->effect_loaded || !filter->effect) {
		if (filter && filter->context)
			obs_source_skip_video_filter(filter->context);
		return;
	}

	obs_source_t *target = obs_filter_get_target(filter->context);

	if (!target) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	const uint32_t width = obs_source_get_base_width(target);
	const uint32_t height = obs_source_get_base_height(target);

	if (width == 0 || height == 0) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;

	struct vec2 pixel_size;
	vec2_set(&pixel_size, 1.0f / (float)width, 1.0f / (float)height);

	if (filter->key_color_param)
		gs_effect_set_vec4(filter->key_color_param, &filter->key_color);

	if (filter->pixel_size_param)
		gs_effect_set_vec2(filter->pixel_size_param, &pixel_size);

	lyn_ultrakey_set_float(filter->similarity_param, filter->similarity);
	lyn_ultrakey_set_float(filter->softness_param, filter->softness);
	lyn_ultrakey_set_float(filter->edge_clean_param, filter->edge_clean);
	lyn_ultrakey_set_float(filter->noise_reject_param, filter->noise_reject);
	lyn_ultrakey_set_float(filter->hair_detail_param, filter->hair_detail);
	lyn_ultrakey_set_float(filter->despill_param, filter->despill);
	lyn_ultrakey_set_float(filter->despill_inside_param, filter->despill_inside);
	lyn_ultrakey_set_float(filter->decontaminate_param, filter->edge_decontaminate);
	lyn_ultrakey_set_float(filter->black_protect_param, filter->black_protect);
	lyn_ultrakey_set_float(filter->debug_view_param, filter->debug_view);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);
}

static obs_properties_t *lyn_ultrakey_get_properties(void *data)
{
	UNUSED_PARAMETER(data);

	/*
     * Properties are deliberately independent of the shader instance.
     * They will still appear if the effect file is missing or invalid.
     */
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_color(props, S_KEY_COLOR, obs_module_text("LynUltraKey.KeyColor"));

	obs_properties_add_int_slider(props, S_SIMILARITY, obs_module_text("LynUltraKey.Similarity"), 0, 1000, 1);

	obs_properties_add_int_slider(props, S_SOFTNESS, obs_module_text("LynUltraKey.Softness"), 0, 1000, 1);

	obs_properties_add_int_slider(props, S_EDGE_CLEAN, obs_module_text("LynUltraKey.EdgeClean"), 0, 1000, 1);

	obs_properties_add_int_slider(props, S_NOISE_REJECT, obs_module_text("LynUltraKey.NoiseReject"), 0, 1000, 1);

	obs_properties_add_int_slider(props, S_HAIR_DETAIL, obs_module_text("LynUltraKey.HairDetail"), 0, 1000, 1);

	obs_properties_add_int_slider(props, S_DESPILL, obs_module_text("LynUltraKey.Despill"), 0, 1000, 1);

	obs_properties_add_int_slider(props, S_DESPILL_INSIDE, obs_module_text("LynUltraKey.DespillInside"), 0, 1000,
				      1);

	obs_properties_add_int_slider(props, S_DECONTAMINATE, obs_module_text("LynUltraKey.Decontaminate"), 0, 1000, 1);

	obs_properties_add_int_slider(props, S_BLACK_PROTECT, obs_module_text("LynUltraKey.BlackProtect"), 0, 1000, 1);

	obs_property_t *debug = obs_properties_add_list(props, S_DEBUG_VIEW, obs_module_text("LynUltraKey.DebugView"),
							OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(debug, obs_module_text("LynUltraKey.DebugFinal"), 0);
	obs_property_list_add_int(debug, obs_module_text("LynUltraKey.DebugMatte"), 1);
	obs_property_list_add_int(debug, obs_module_text("LynUltraKey.DebugEdge"), 2);
	obs_property_list_add_int(debug, obs_module_text("LynUltraKey.DebugSpill"), 3);

	return props;
}

static void lyn_ultrakey_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_KEY_COLOR, 0x00CC33);
	obs_data_set_default_int(settings, S_SIMILARITY, 430);
	obs_data_set_default_int(settings, S_SOFTNESS, 100);
	obs_data_set_default_int(settings, S_EDGE_CLEAN, 400);
	obs_data_set_default_int(settings, S_NOISE_REJECT, 650);
	obs_data_set_default_int(settings, S_HAIR_DETAIL, 350);
	obs_data_set_default_int(settings, S_DESPILL, 900);
	obs_data_set_default_int(settings, S_DESPILL_INSIDE, 650);
	obs_data_set_default_int(settings, S_DECONTAMINATE, 700);
	obs_data_set_default_int(settings, S_BLACK_PROTECT, 250);
	obs_data_set_default_int(settings, S_DEBUG_VIEW, 0);
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
