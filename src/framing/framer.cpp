#include "framing/framer.hpp"

#include <algorithm>

namespace autoframe::framing {
namespace {

// Zoom below 1 would ask for a crop larger than the bounds.
constexpr float kMinZoom = 1.0f;

} // namespace

Vec2 Framer::max_crop_size() const
{
	const Rect &b = config_.bounds;
	if (b.width <= 0.0f || b.height <= 0.0f || config_.aspect <= 0.0f)
		return {};

	if (b.width / b.height > config_.aspect)
		return {b.height * config_.aspect, b.height};
	return {b.width, b.width / config_.aspect};
}

float Framer::clamp_width(float width) const
{
	const float max_width = max_crop_size().x;
	const float min_zoom = std::max(kMinZoom, config_.min_zoom);
	const float max_zoom = std::max(min_zoom, config_.max_zoom);
	return std::clamp(width, max_width / max_zoom, max_width / min_zoom);
}

Rect Framer::frame_unclamped(const Rect &subject) const
{
	const Vec2 center = subject.center() + Vec2{config_.offset_x * subject.width, config_.offset_y * subject.height};
	float width = subject.width * (1.0f + 2.0f * config_.padding_x);
	const float height = subject.height * (1.0f + 2.0f * config_.padding_y);

	// Grow the narrower side so the whole padded subject fits the output aspect.
	width = std::max(width, height * config_.aspect);
	width = clamp_width(width);

	return Rect::from_center(center, width, width / config_.aspect);
}

Rect Framer::idle() const
{
	const float width = clamp_width(max_crop_size().x);
	return clamp(Rect::from_center(config_.bounds.center(), width, width / config_.aspect));
}

Rect Framer::clamp(const Rect &crop) const
{
	const Rect &b = config_.bounds;
	const Vec2 max_size = max_crop_size();
	Rect out = crop;
	if (out.width > max_size.x) {
		out.width = max_size.x;
		out.height = max_size.y;
	}
	out.x = std::clamp(crop.center().x - out.width * 0.5f, b.x, b.right() - out.width);
	out.y = std::clamp(crop.center().y - out.height * 0.5f, b.y, b.bottom() - out.height);
	return out;
}

} // namespace autoframe::framing
