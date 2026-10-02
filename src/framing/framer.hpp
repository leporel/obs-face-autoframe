// Computes the crop rectangle that frames a subject: offset, padding, output aspect, zoom limits, bounds.
#pragma once

#include "framing/types.hpp"

namespace autoframe::framing {

struct FramerConfig {
	// Space added on each side of the subject, as a fraction of the subject size.
	float padding_x = 0.33f;
	float padding_y = 0.33f;
	// Shift of the framing center as a fraction of the subject size (positive = right / down).
	float offset_x = 0.0f;
	float offset_y = -0.075f;
	// 1 = the crop may cover the whole bounds; 3 = the crop is never smaller than a third of them.
	float min_zoom = 1.0f;
	float max_zoom = 3.0f;
	// Output width / height; the crop always has this aspect so the output never stretches.
	float aspect = 16.0f / 9.0f;
	// The crop never leaves this area (source pixels).
	Rect bounds{0.0f, 0.0f, 1920.0f, 1080.0f};
};

class Framer {
public:
	void configure(const FramerConfig &config) { config_ = config; }
	[[nodiscard]] const FramerConfig &config() const { return config_; }

	// Crop that frames `subject` within all limits.
	[[nodiscard]] Rect frame(const Rect &subject) const { return clamp(frame_unclamped(subject)); }

	// Same, but not moved inside the bounds: where the camera would ideally be centered.
	[[nodiscard]] Rect frame_unclamped(const Rect &subject) const;

	// Crop used when nobody is framed: the widest allowed crop, centered in the bounds.
	[[nodiscard]] Rect idle() const;

	// Moves `crop` inside the bounds (and shrinks it only if it is larger than the bounds).
	[[nodiscard]] Rect clamp(const Rect &crop) const;

private:
	// Largest crop of the output aspect that fits the bounds.
	[[nodiscard]] Vec2 max_crop_size() const;
	[[nodiscard]] float clamp_width(float width) const;

	FramerConfig config_;
};

} // namespace autoframe::framing
