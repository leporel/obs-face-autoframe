// Produces the small SDR BGRA frame that is handed to the detector, entirely on the GPU.
#pragma once

#include "detect/detection.hpp"

#include <obs-module.h>

namespace autoframe {

// Geometry of the detection frame. Its size depends only on the source size and the detection width, so changing
// the detection area never changes it (a size change forces the SDK to reload its model). The area is scaled
// uniformly to fit and placed at the top-left; `content` is that placement normalized to the frame.
struct DetectLayout {
	uint32_t width = 0;
	uint32_t height = 0;
	NormRect content = kFullFrame;
};

// `max_width` 0 means native resolution; the frame is never larger than the source.
[[nodiscard]] DetectLayout detection_layout(uint32_t source_width, uint32_t source_height, const NormRect &roi,
					    uint32_t max_width);

class DetectFrame {
public:
	DetectFrame() = default;
	~DetectFrame();
	DetectFrame(const DetectFrame &) = delete;
	DetectFrame &operator=(const DetectFrame &) = delete;

	// Graphics context required. Returns false if the effect file cannot be compiled.
	bool load_effect();
	[[nodiscard]] bool effect_loaded() const { return effect_ != nullptr; }

	// Graphics thread. Draws the `roi` part of `source` (captured in `space`) into the layout's content area,
	// tonemaps HDR to SDR and sRGB-encodes it. Returns the BGRA texture or nullptr on failure.
	gs_texture_t *render(gs_texture_t *source, gs_color_space space, const NormRect &roi,
			     const DetectLayout &layout);

private:
	gs_effect_t *effect_ = nullptr;
	gs_eparam_t *image_param_ = nullptr;
	gs_eparam_t *multiplier_param_ = nullptr;
	gs_texrender_t *render_ = nullptr;
};

} // namespace autoframe
