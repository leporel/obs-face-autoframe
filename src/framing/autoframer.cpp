#include "framing/autoframer.hpp"

#include <algorithm>

namespace autoframe::framing {
namespace {

// Longest time step fed to the camera. Larger gaps (source hidden, stalls) would otherwise make the spring
// snap to the target in one frame.
constexpr double kMaxStepS = 0.1;

bool same_geometry(const FramerConfig &a, const FramerConfig &b)
{
	return a.aspect == b.aspect && a.bounds.x == b.bounds.x && a.bounds.y == b.bounds.y &&
	       a.bounds.width == b.bounds.width && a.bounds.height == b.bounds.height;
}

} // namespace

void Autoframer::configure(const AutoframerConfig &config)
{
	const bool geometry_changed = !same_geometry(config.framer, config_.framer);
	config_ = config;
	tracker_.configure(config.tracker);
	selector_.configure(config.selector);
	framer_.configure(config.framer);
	motion_.configure(config.motion);

	if (geometry_changed && motion_.initialized()) {
		motion_.reset(framer_.idle());
		last_subject_s_.reset();
	}
}

void Autoframer::on_detections(std::span<const Observation> observations, double now_s)
{
	tracker_.update(observations, now_s);
}

void Autoframer::reset_lock()
{
	selector_.reset();
}

void Autoframer::reset(double now_s)
{
	tracker_.reset();
	selector_.reset();
	// Keep the current framing for the usual hold time instead of zooming out while tracking restarts.
	if (last_subject_s_)
		last_subject_s_ = now_s;
}

Rect Autoframer::choose_target(const std::optional<Subject> &subject, double now_s)
{
	if (subject) {
		// Look ahead along the motion, fading the prediction out as the detection gets older.
		const double lost_timeout = std::max(config_.tracker.lost_timeout_s, 1e-3);
		const auto freshness = static_cast<float>(std::clamp(1.0 - subject->staleness_s / lost_timeout, 0.0, 1.0));
		const Vec2 shift = subject->velocity * static_cast<float>(config_.prediction_s) * freshness;
		const Rect predicted{subject->box.x + shift.x, subject->box.y + shift.y, subject->box.width,
				     subject->box.height};

		state_.subject = predicted;
		last_subject_s_ = now_s;
		const Rect desired = framer_.frame_unclamped(predicted);
		last_desired_center_ = desired.center();
		last_target_ = framer_.clamp(desired);
		desired_center_ = last_desired_center_;
		return last_target_;
	}

	state_.subject.reset();
	if (last_subject_s_) {
		const bool holding = now_s - *last_subject_s_ < config_.hold_time_s;
		if (holding || config_.idle == IdleBehavior::KeepLast) {
			desired_center_ = last_desired_center_;
			return last_target_;
		}
	}
	const Rect idle = framer_.idle();
	desired_center_ = idle.center();
	return idle;
}

const FrameState &Autoframer::update(double now_s)
{
	const double dt = last_update_s_ ? std::clamp(now_s - *last_update_s_, 0.0, kMaxStepS) : 0.0;
	last_update_s_ = now_s;

	tracker_.advance(now_s);
	const std::optional<Subject> subject =
		selector_.select(tracker_.tracks(), framer_.config().bounds.center(), now_s);
	state_.target = choose_target(subject, now_s);

	// Start wide and glide in, rather than snapping to the first face.
	if (!motion_.initialized())
		motion_.reset(framer_.idle());
	state_.crop = framer_.clamp(motion_.update(state_.target, desired_center_, dt));
	return state_;
}

} // namespace autoframe::framing
