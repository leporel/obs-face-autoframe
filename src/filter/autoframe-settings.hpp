// Filter settings: defaults, parsing into a plain struct, property UI.
#pragma once

#include "detect/detection.hpp"
#include "framing/autoframer.hpp"
#include "render/source-capture.hpp"

#include <obs-module.h>

#include <cstdint>
#include <string>

namespace autoframe {

enum class OutputMode : int {
	Source = 0, // same size as the webcam
	Custom = 1,
};

struct Settings {
	DetectTarget target = DetectTarget::Face;
	// Run detection on every N-th frame; frames in between reuse the last result.
	uint32_t detect_interval = 2;
	// Width of the frame the detector works on; 0 = native resolution.
	uint32_t detect_max_width = 640;
	// The camera never shows anything outside this area and searches faces only inside it (normalized to the
	// source). Fully zoomed out, the crop is the largest output-aspect rectangle inside it.
	NormRect frame_area = kFullFrame;
	// Detections below this confidence (0..1) are ignored.
	float min_confidence = 0.5f;
	bool debug_view = false;
	OutputMode output_mode = OutputMode::Source;
	uint32_t output_width = 1920;
	uint32_t output_height = 1080;
	ScaleFilter scale_filter = ScaleFilter::Bicubic;
	// Framing pipeline settings. Bounds and aspect are filled in by the filter from the frame area and output size.
	framing::AutoframerConfig framing;
};

void set_default_settings(obs_data_t *data);
[[nodiscard]] Settings read_settings(obs_data_t *data);

// `status` is shown at the top of the properties (SDK / GPU state, errors).
// `reset_lock` is invoked by the "Reset lock" button with `filter` as its data.
[[nodiscard]] obs_properties_t *create_properties(const std::string &status, obs_property_clicked_t reset_lock,
						  void *filter);

// Builds the frame area from its size (percent of the frame, at least 10) and the offset of its center from
// the frame center (percent, -50..50). The area is shifted back inside the frame if the offset pushes it out.
[[nodiscard]] NormRect rect_from_size_and_offset(double width_percent, double height_percent,
						 double offset_x_percent, double offset_y_percent);

} // namespace autoframe
