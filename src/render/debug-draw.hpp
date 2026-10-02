// Simple outline drawing for the debug view (setup aid, not part of the normal output).
#pragma once

#include "framing/types.hpp"

#include <cstdint>

namespace autoframe {

// Colors are 0xAARRGGBB.
namespace debug_color {
constexpr uint32_t kDetection = 0xFFFF3030;
constexpr uint32_t kFrameArea = 0xFF30E040;
constexpr uint32_t kTrackSelected = 0xFFFFD020;
constexpr uint32_t kTrackIgnored = 0xFF808080;
constexpr uint32_t kSubject = 0xFF20D0FF;
constexpr uint32_t kCrop = 0xFFFFFFFF;
} // namespace debug_color

// Graphics thread. Draws a rectangle outline (source pixels) in the current render target coordinates.
void draw_rect_outline(const framing::Rect &rect, float thickness, uint32_t argb);

// Graphics thread. Draws `value` as seven-segment digits with the top-left corner at (x, y).
void draw_number(uint32_t value, float x, float y, float height, uint32_t argb);

// Outline thickness that stays visible regardless of source resolution.
[[nodiscard]] float outline_thickness(uint32_t frame_width);

} // namespace autoframe
