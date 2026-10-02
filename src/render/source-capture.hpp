// Captures the filter input once per frame into a texture that can be drawn many times
// (preview, program, projectors) and fed to the detector.
#pragma once

#include "framing/types.hpp"

#include <obs-module.h>

namespace autoframe {

// Sampling used when the crop is scaled to the output. Bilinear is the cheapest; Bicubic and Lanczos keep more
// detail when zoomed in (Lanczos is the sharpest but can show slight ringing on hard edges).
enum class ScaleFilter : int {
	Bilinear = 0,
	Bicubic = 1,
	Lanczos = 2,
};

class SourceCapture {
public:
	SourceCapture() = default;
	~SourceCapture();
	SourceCapture(const SourceCapture &) = delete;
	SourceCapture &operator=(const SourceCapture &) = delete;

	// Graphics thread. Renders the filter target into the internal texture in its preferred color space.
	bool capture(obs_source_t *filter, uint32_t width, uint32_t height);

	// Draws the captured frame (or only `crop`, in source pixels) scaled to width x height, converting to the
	// current render target color space.
	void draw(uint32_t width, uint32_t height, const framing::Rect *crop = nullptr) const;

	void set_scale_filter(ScaleFilter filter) { scale_filter_ = filter; }

	[[nodiscard]] gs_texture_t *texture() const;
	[[nodiscard]] gs_color_space space() const { return space_; }

	// Implements obs_source_info::video_get_color_space for the captured content.
	[[nodiscard]] gs_color_space output_space(size_t count, const gs_color_space *preferred) const;

private:
	// Base effect for the selected scale filter with its per-texture parameters set.
	[[nodiscard]] gs_effect_t *scale_effect(gs_texture_t *tex) const;

	gs_texrender_t *render_ = nullptr;
	ScaleFilter scale_filter_ = ScaleFilter::Bicubic;
	gs_color_space space_ = GS_CS_SRGB;
	bool has_frame_ = false;
};

} // namespace autoframe
