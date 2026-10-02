// Backend-independent detection types shared by detectors, framing and rendering.
#pragma once

#include <cstdint>

namespace autoframe {

enum class DetectTarget : int {
	Face = 0,
	Body = 1,
};

// Axis-aligned rectangle normalized to [0, 1] relative to a frame.
struct NormRect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 1.0f;
	float height = 1.0f;
};

constexpr NormRect kFullFrame{};

struct Detection {
	NormRect box;
	float confidence = 0.0f;
};

// NVIDIA FaceBox/Body detection report at most 8 objects per frame.
constexpr uint32_t kMaxDetections = 8;

// Converts a box normalized to `roi` into a box normalized to the full frame.
[[nodiscard]] constexpr NormRect map_from_roi(const NormRect &box, const NormRect &roi)
{
	return {roi.x + box.x * roi.width, roi.y + box.y * roi.height, box.width * roi.width,
		box.height * roi.height};
}

// Inverse of map_from_roi: expresses `box` relative to `region` (both normalized to the same frame).
[[nodiscard]] constexpr NormRect relative_to(const NormRect &box, const NormRect &region)
{
	return {(box.x - region.x) / region.width, (box.y - region.y) / region.height, box.width / region.width,
		box.height / region.height};
}

[[nodiscard]] constexpr bool is_full_frame(const NormRect &rect)
{
	return rect.x <= 0.0f && rect.y <= 0.0f && rect.width >= 1.0f && rect.height >= 1.0f;
}

} // namespace autoframe
