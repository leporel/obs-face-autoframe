// Smooth virtual camera: moves the crop towards a target without jumps, jitter or overshoot.
#pragma once

#include "framing/types.hpp"

namespace autoframe::framing {

struct MotionConfig {
	// Time to (mostly) reach a new position / size; 0 snaps immediately.
	double position_smooth_s = 0.66;
	double zoom_smooth_s = 1.0;
	// The target center must leave this zone (fraction of the crop size around the current aim) before the camera
	// moves, so small head movements do not shake the picture.
	float dead_zone = 0.1f;
	// The target size must change by more than this fraction before the camera zooms.
	float zoom_stability = 0.1f;
	// Speed limit in crop widths per second; 0 = unlimited.
	float max_pan_speed = 0.0f;
};

class CameraMotion {
public:
	void configure(const MotionConfig &config) { config_ = config; }

	// Places the camera at `crop` with no motion.
	void reset(const Rect &crop);

	// Advances the camera by `dt` seconds towards `target` (which must already have the output aspect).
	// `desired_center` is where the camera would aim without the frame-area limits; the dead zone is measured on
	// it, so a target pinned against the frame-area edge is still reached in full.
	[[nodiscard]] Rect update(const Rect &target, Vec2 desired_center, double dt);
	[[nodiscard]] Rect update(const Rect &target, double dt) { return update(target, target.center(), dt); }

	[[nodiscard]] bool initialized() const { return initialized_; }

private:
	struct Channel {
		float value = 0.0f;
		float velocity = 0.0f;
	};

	// Per-axis dead zone: decides where the camera currently aims.
	struct Aim {
		// Desired (unlimited) position the camera last re-aimed at; the dead zone is centered on it.
		float anchor = 0.0f;
		// Reachable position the camera is heading to.
		float goal = 0.0f;
		bool moving = false;
	};

	void update_aim(Aim &aim, float desired, float target, float current, float zone) const;

	MotionConfig config_;
	bool initialized_ = false;
	Channel x_, y_, width_;
	Aim aim_x_, aim_y_;
	float goal_width_ = 0.0f;
};

// Critically damped smoothing step (no overshoot), frame-rate independent.
// `max_speed` <= 0 means unlimited. Updates `velocity` and returns the new value.
[[nodiscard]] float smooth_damp(float current, float target, float &velocity, double smooth_time, float max_speed,
				double dt);

} // namespace autoframe::framing
