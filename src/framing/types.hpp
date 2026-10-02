// Geometry types of the framing core. Everything is in source pixels; no OBS or detector dependencies.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace autoframe::framing {

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b)
{
	return {a.x + b.x, a.y + b.y};
}
constexpr Vec2 operator-(Vec2 a, Vec2 b)
{
	return {a.x - b.x, a.y - b.y};
}
constexpr Vec2 operator*(Vec2 v, float s)
{
	return {v.x * s, v.y * s};
}

inline float length(Vec2 v)
{
	return std::hypot(v.x, v.y);
}

struct Rect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;

	[[nodiscard]] constexpr float right() const { return x + width; }
	[[nodiscard]] constexpr float bottom() const { return y + height; }
	[[nodiscard]] constexpr float area() const { return width * height; }
	[[nodiscard]] constexpr Vec2 center() const { return {x + width * 0.5f, y + height * 0.5f}; }

	[[nodiscard]] static constexpr Rect from_center(Vec2 c, float w, float h)
	{
		return {c.x - w * 0.5f, c.y - h * 0.5f, w, h};
	}
};

[[nodiscard]] inline Rect union_of(const Rect &a, const Rect &b)
{
	const float left = std::min(a.x, b.x);
	const float top = std::min(a.y, b.y);
	return {left, top, std::max(a.right(), b.right()) - left, std::max(a.bottom(), b.bottom()) - top};
}

// Intersection over union, 0 when the rectangles do not overlap.
[[nodiscard]] inline float iou(const Rect &a, const Rect &b)
{
	const float w = std::min(a.right(), b.right()) - std::max(a.x, b.x);
	const float h = std::min(a.bottom(), b.bottom()) - std::max(a.y, b.y);
	if (w <= 0.0f || h <= 0.0f)
		return 0.0f;
	const float intersection = w * h;
	return intersection / (a.area() + b.area() - intersection);
}

// A detection in source pixels.
struct Observation {
	Rect box;
	float confidence = 0.0f;
};

} // namespace autoframe::framing
