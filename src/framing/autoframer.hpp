// Full framing pipeline: detections -> tracks -> subject -> target crop -> smoothed crop.
#pragma once

#include "framing/framer.hpp"
#include "framing/motion.hpp"
#include "framing/target-selector.hpp"
#include "framing/tracker.hpp"

#include <optional>
#include <span>

namespace autoframe::framing {

enum class IdleBehavior : int {
	ZoomOut = 0,  // return to the widest crop
	KeepLast = 1, // stay where the last person was
};

struct AutoframerConfig {
	TrackerConfig tracker;
	SelectorConfig selector;
	FramerConfig framer;
	MotionConfig motion;
	// Look-ahead along the subject's velocity, seconds; compensates for detection latency.
	double prediction_s = 0.1;
	// How long the last framing is held after the subject disappears.
	double hold_time_s = 2.0;
	IdleBehavior idle = IdleBehavior::ZoomOut;
};

struct FrameState {
	// Smoothed crop to render.
	Rect crop;
	// Where the camera is heading.
	Rect target;
	// What is framed (union of selected tracks, with prediction applied), if anyone.
	std::optional<Rect> subject;
};

class Autoframer {
public:
	// Bounds or aspect changes restart the camera from the idle crop.
	void configure(const AutoframerConfig &config);

	// Feeds a detection result for the frame taken at `now_s`.
	void on_detections(std::span<const Observation> observations, double now_s);

	// Advances to `now_s` (video clock, seconds). Call once per frame.
	const FrameState &update(double now_s);

	// Forgets the current / locked person.
	void reset_lock();

	// Restarts tracking (e.g. after the detector is recreated or the source was hidden). The camera keeps its
	// position and the last framing is held for the hold time from `now_s`.
	void reset(double now_s);

	[[nodiscard]] std::span<const Track> tracks() const { return tracker_.tracks(); }
	[[nodiscard]] std::span<const uint32_t> selected_ids() const { return selector_.selected_ids(); }
	[[nodiscard]] const FrameState &state() const { return state_; }

private:
	Rect choose_target(const std::optional<Subject> &subject, double now_s);

	AutoframerConfig config_;
	Tracker tracker_;
	TargetSelector selector_;
	Framer framer_;
	CameraMotion motion_;
	FrameState state_;

	std::optional<double> last_update_s_;
	std::optional<double> last_subject_s_;
	Rect last_target_;
	// Unlimited aim positions (before the frame-area clamp), fed to the camera's dead zone.
	Vec2 last_desired_center_;
	Vec2 desired_center_;
};

} // namespace autoframe::framing
