#include "render/source-capture.hpp"
#include "render/color.hpp"

namespace autoframe {
namespace {

// Picks the default-effect technique that converts from the captured space to the render target space.
// Mirrors the conversion table used by OBS' own caching filters (e.g. gpu-delay).
const char *draw_technique(gs_color_space current, gs_color_space source, float &multiplier)
{
	multiplier = 1.0f;
	const float sdr_white = obs_get_video_sdr_white_level();

	switch (source) {
	case GS_CS_SRGB:
	case GS_CS_SRGB_16F:
		if (current == GS_CS_709_SCRGB) {
			multiplier = sdr_white / kScRgbReferenceWhite;
			return "DrawMultiply";
		}
		return "Draw";
	case GS_CS_709_EXTENDED:
		if (current == GS_CS_SRGB || current == GS_CS_SRGB_16F)
			return "DrawTonemap";
		if (current == GS_CS_709_SCRGB) {
			multiplier = sdr_white / kScRgbReferenceWhite;
			return "DrawMultiply";
		}
		return "Draw";
	case GS_CS_709_SCRGB:
		if (current == GS_CS_SRGB || current == GS_CS_SRGB_16F) {
			multiplier = kScRgbReferenceWhite / sdr_white;
			return "DrawMultiplyTonemap";
		}
		if (current == GS_CS_709_EXTENDED) {
			multiplier = kScRgbReferenceWhite / sdr_white;
			return "DrawMultiply";
		}
		return "Draw";
	}
	return "Draw";
}

} // namespace

SourceCapture::~SourceCapture()
{
	gs_texrender_destroy(render_);
}

bool SourceCapture::capture(obs_source_t *filter, uint32_t width, uint32_t height)
{
	obs_source_t *target = obs_filter_get_target(filter);
	obs_source_t *parent = obs_filter_get_parent(filter);
	if (!target || !parent || width == 0 || height == 0)
		return false;

	const gs_color_space preferred[] = {GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED};
	const gs_color_space space = obs_source_get_color_space(target, OBS_COUNTOF(preferred), preferred);
	const gs_color_format format = gs_get_format_from_space(space);

	if (!render_ || gs_texrender_get_format(render_) != format) {
		gs_texrender_destroy(render_);
		render_ = gs_texrender_create(format, GS_ZS_NONE);
		if (!render_)
			return false;
	}
	gs_texrender_reset(render_);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	const bool rendered = gs_texrender_begin_with_color_space(render_, width, height, space);
	if (rendered) {
		const uint32_t parent_flags = obs_source_get_output_flags(target);
		const bool custom_draw = (parent_flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
		const bool async = (parent_flags & OBS_SOURCE_ASYNC) != 0;

		vec4 clear_color;
		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);

		if (target == parent && !custom_draw && !async)
			obs_source_default_render(target);
		else
			obs_source_video_render(target);

		gs_texrender_end(render_);
		space_ = space;
	}

	gs_blend_state_pop();
	has_frame_ = rendered;
	return rendered;
}

gs_effect_t *SourceCapture::scale_effect(gs_texture_t *tex) const
{
	obs_base_effect type = OBS_EFFECT_DEFAULT;
	switch (scale_filter_) {
	case ScaleFilter::Bicubic:
		type = OBS_EFFECT_BICUBIC;
		break;
	case ScaleFilter::Lanczos:
		type = OBS_EFFECT_LANCZOS;
		break;
	case ScaleFilter::Bilinear:
		break;
	}

	gs_effect_t *effect = obs_get_base_effect(type);
	if (type == OBS_EFFECT_DEFAULT)
		return effect;

	// Bicubic and Lanczos sample neighbouring texels, so they need the texel size of the source.
	vec2 dimension;
	vec2 dimension_i;
	vec2_set(&dimension, static_cast<float>(gs_texture_get_width(tex)), static_cast<float>(gs_texture_get_height(tex)));
	vec2_set(&dimension_i, 1.0f / dimension.x, 1.0f / dimension.y);
	gs_effect_set_vec2(gs_effect_get_param_by_name(effect, "base_dimension"), &dimension);
	gs_effect_set_vec2(gs_effect_get_param_by_name(effect, "base_dimension_i"), &dimension_i);
	gs_effect_set_float(gs_effect_get_param_by_name(effect, "undistort_factor"), 1.0f);
	return effect;
}

void SourceCapture::draw(uint32_t width, uint32_t height, const framing::Rect *crop) const
{
	gs_texture_t *tex = texture();
	if (!tex)
		return;

	const uint32_t source_width = gs_texture_get_width(tex);
	const uint32_t source_height = gs_texture_get_height(tex);
	if (crop && (crop->width <= 0.0f || crop->height <= 0.0f))
		crop = nullptr;

	float multiplier = 1.0f;
	const char *technique = draw_technique(gs_get_color_space(), space_, multiplier);
	gs_effect_t *effect = scale_effect(tex);

	const bool previous_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_effect_set_texture_srgb(gs_effect_get_param_by_name(effect, "image"), tex);
	gs_effect_set_float(gs_effect_get_param_by_name(effect, "multiplier"), multiplier);

	gs_matrix_push();
	if (crop) {
		// Scale and shift the whole source so the crop fills the output; float math keeps sub-pixel camera
		// motion smooth. The last transform call applies first: shift by -crop, then scale.
		gs_matrix_scale3f(static_cast<float>(width) / crop->width, static_cast<float>(height) / crop->height,
				  1.0f);
		gs_matrix_translate3f(-crop->x, -crop->y, 0.0f);
	} else {
		gs_matrix_scale3f(static_cast<float>(width) / static_cast<float>(source_width),
				  static_cast<float>(height) / static_cast<float>(source_height), 1.0f);
	}
	while (gs_effect_loop(effect, technique))
		gs_draw_sprite(tex, 0, source_width, source_height);
	gs_matrix_pop();

	gs_enable_framebuffer_srgb(previous_srgb);
}

gs_texture_t *SourceCapture::texture() const
{
	return (render_ && has_frame_) ? gs_texrender_get_texture(render_) : nullptr;
}

gs_color_space SourceCapture::output_space(size_t count, const gs_color_space *preferred) const
{
	if (!has_frame_ || count == 0)
		return count > 0 ? preferred[0] : GS_CS_SRGB;

	for (size_t i = 0; i < count; ++i) {
		if (preferred[i] == space_)
			return space_;
	}
	return preferred[count - 1];
}

} // namespace autoframe
