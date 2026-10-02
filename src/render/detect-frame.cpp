#include "render/detect-frame.hpp"
#include "render/color.hpp"

#include "util/log.hpp"

#include <util/bmem.h>

#include <algorithm>
#include <cmath>

namespace autoframe {
namespace {

constexpr const char *kEffectFile = "effects/autoframe.effect";

const char *detect_technique(gs_color_space space, float &multiplier)
{
	multiplier = 1.0f;
	switch (space) {
	case GS_CS_709_EXTENDED:
		return "DetectTonemap";
	case GS_CS_709_SCRGB:
		multiplier = kScRgbReferenceWhite / obs_get_video_sdr_white_level();
		return "DetectMultiplyTonemap";
	default:
		return "DetectSdr";
	}
}

} // namespace

DetectFrame::~DetectFrame()
{
	gs_texrender_destroy(render_);
	gs_effect_destroy(effect_);
}

bool DetectFrame::load_effect()
{
	if (effect_)
		return true;

	char *path = obs_module_file(kEffectFile);
	if (!path) {
		obs_log(LOG_ERROR, "missing data file %s", kEffectFile);
		return false;
	}

	char *errors = nullptr;
	effect_ = gs_effect_create_from_file(path, &errors);
	if (!effect_)
		obs_log(LOG_ERROR, "failed to compile %s: %s", path, errors ? errors : "unknown error");
	bfree(errors);
	bfree(path);
	if (!effect_)
		return false;

	image_param_ = gs_effect_get_param_by_name(effect_, "image");
	multiplier_param_ = gs_effect_get_param_by_name(effect_, "multiplier");
	return true;
}

gs_texture_t *DetectFrame::render(gs_texture_t *source, gs_color_space space, const NormRect &roi,
				  const DetectLayout &layout)
{
	if (!effect_ || !source || layout.width == 0 || layout.height == 0)
		return nullptr;

	const uint32_t source_width = gs_texture_get_width(source);
	const uint32_t source_height = gs_texture_get_height(source);
	const auto sw = static_cast<float>(source_width);
	const auto sh = static_cast<float>(source_height);
	const auto content_width = static_cast<int>(std::lround(layout.content.width * layout.width));
	const auto content_height = static_cast<int>(std::lround(layout.content.height * layout.height));

	if (!render_) {
		render_ = gs_texrender_create(GS_BGRA_UNORM, GS_ZS_NONE);
		if (!render_)
			return nullptr;
	}
	gs_texrender_reset(render_);

	float multiplier = 1.0f;
	const char *technique = detect_technique(space, multiplier);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	const bool previous_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);

	const bool rendered = gs_texrender_begin_with_color_space(render_, layout.width, layout.height, GS_CS_SRGB);
	if (rendered) {
		vec4 black;
		vec4_set(&black, 0.0f, 0.0f, 0.0f, 1.0f);
		gs_clear(GS_CLEAR_COLOR, &black, 0.0f, 0);

		// The viewport limits drawing to the content area and the projection covers only the ROI in source
		// pixels, so drawing the whole source crops and scales it into place.
		gs_set_viewport(0, 0, content_width, content_height);
		gs_ortho(roi.x * sw, (roi.x + roi.width) * sw, roi.y * sh, (roi.y + roi.height) * sh, -100.0f, 100.0f);
		gs_effect_set_texture_srgb(image_param_, source);
		gs_effect_set_float(multiplier_param_, multiplier);
		while (gs_effect_loop(effect_, technique))
			gs_draw_sprite(source, 0, source_width, source_height);
		gs_texrender_end(render_);
	}

	gs_enable_framebuffer_srgb(previous_srgb);
	gs_blend_state_pop();
	return rendered ? gs_texrender_get_texture(render_) : nullptr;
}

DetectLayout detection_layout(uint32_t source_width, uint32_t source_height, const NormRect &roi, uint32_t max_width)
{
	if (source_width == 0 || source_height == 0)
		return {};

	const double sw = source_width;
	const double sh = source_height;
	const double frame_width = max_width == 0 ? sw : std::min(sw, static_cast<double>(max_width));
	const double frame_height = std::max(1.0, std::round(sh * frame_width / sw));

	const double roi_width = sw * roi.width;
	const double roi_height = sh * roi.height;
	if (roi_width < 1.0 || roi_height < 1.0)
		return {};

	// Uniform scale so the detector sees undistorted faces; small areas are enlarged to use the whole frame.
	const double scale = std::min(frame_width / roi_width, frame_height / roi_height);
	const double content_width = std::clamp(std::round(roi_width * scale), 1.0, frame_width);
	const double content_height = std::clamp(std::round(roi_height * scale), 1.0, frame_height);

	DetectLayout layout;
	layout.width = static_cast<uint32_t>(frame_width);
	layout.height = static_cast<uint32_t>(frame_height);
	layout.content = {0.0f, 0.0f, static_cast<float>(content_width / frame_width),
			  static_cast<float>(content_height / frame_height)};
	return layout;
}

} // namespace autoframe
