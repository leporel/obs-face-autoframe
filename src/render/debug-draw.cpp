#include "render/debug-draw.hpp"

#include <obs-module.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace autoframe {
namespace {

// Outline thickness relative to the frame width (about 3 px at 1920 wide) with a 2 px floor for small sources.
constexpr float kThicknessPerWidth = 1.0f / 640.0f;
constexpr float kMinThickness = 2.0f;

// Seven-segment digits: bit 0..6 = segments a (top), b (top right), c (bottom right), d (bottom),
// e (bottom left), f (top left), g (middle).
constexpr std::array<uint8_t, 10> kDigitSegments{0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
constexpr float kDigitAspect = 0.5f;      // width / height
constexpr float kSegmentThickness = 0.14f; // of the digit height
constexpr float kDigitSpacing = 0.25f;    // gap between digits, of the digit height
constexpr uint32_t kDecimalBase = 10;

// Solid-color quads for one draw call sequence.
class SolidPainter {
public:
	explicit SolidPainter(uint32_t argb)
		: solid_(obs_get_base_effect(OBS_EFFECT_SOLID)),
		  color_(gs_effect_get_param_by_name(solid_, "color")),
		  argb_(argb)
	{
	}

	void quad(float x, float y, float width, float height) const
	{
		const auto cx = static_cast<uint32_t>(std::max(1.0f, std::round(width)));
		const auto cy = static_cast<uint32_t>(std::max(1.0f, std::round(height)));

		// OBS resets effect parameters to their defaults (white for "color") when a technique ends,
		// so the color has to be set before every draw.
		gs_effect_set_color(color_, argb_);

		gs_matrix_push();
		gs_matrix_translate3f(x, y, 0.0f);
		while (gs_effect_loop(solid_, "Solid"))
			gs_draw_sprite(nullptr, 0, cx, cy);
		gs_matrix_pop();
	}

private:
	gs_effect_t *solid_;
	gs_eparam_t *color_;
	uint32_t argb_;
};

void draw_digit(const SolidPainter &painter, uint32_t digit, float x, float y, float height)
{
	const float w = height * kDigitAspect;
	const float t = height * kSegmentThickness;
	const float half = height * 0.5f;
	const uint8_t mask = kDigitSegments[digit % kDecimalBase];

	const auto segment = [&](int bit, float sx, float sy, float sw, float sh) {
		if (mask & (1u << bit))
			painter.quad(x + sx, y + sy, sw, sh);
	};
	segment(0, 0, 0, w, t);                  // a
	segment(1, w - t, 0, t, half);           // b
	segment(2, w - t, half, t, half);        // c
	segment(3, 0, height - t, w, t);         // d
	segment(4, 0, half, t, half);            // e
	segment(5, 0, 0, t, half);               // f
	segment(6, 0, half - t * 0.5f, w, t);    // g
}

} // namespace

void draw_rect_outline(const framing::Rect &rect, float thickness, uint32_t argb)
{
	const SolidPainter painter(argb);
	const float right = rect.x + rect.width - thickness;
	const float bottom = rect.y + rect.height - thickness;

	painter.quad(rect.x, rect.y, rect.width, thickness);
	painter.quad(rect.x, bottom, rect.width, thickness);
	painter.quad(rect.x, rect.y, thickness, rect.height);
	painter.quad(right, rect.y, thickness, rect.height);
}

void draw_number(uint32_t value, float x, float y, float height, uint32_t argb)
{
	std::array<uint32_t, 10> digits{};
	size_t count = 0;
	do {
		digits[count++] = value % kDecimalBase;
		value /= kDecimalBase;
	} while (value > 0 && count < digits.size());

	const SolidPainter painter(argb);
	const float advance = height * (kDigitAspect + kDigitSpacing);
	for (size_t i = 0; i < count; ++i)
		draw_digit(painter, digits[count - 1 - i], x + advance * static_cast<float>(i), y, height);
}

float outline_thickness(uint32_t frame_width)
{
	return std::max(kMinThickness, std::round(static_cast<float>(frame_width) * kThicknessPerWidth));
}

} // namespace autoframe
