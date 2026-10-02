#include "framing/motion.hpp"

#include <algorithm>
#include <cmath>

namespace autoframe::framing {
namespace {

// Once moving, the camera keeps following until the target is this close (fraction of the dead zone), so it
// re-centers instead of stopping right at the dead-zone edge.
constexpr float kSettleFraction = 0.25f;

} // namespace

float smooth_damp(float current, float target, float &velocity, double smooth_time, float max_speed, double dt)
{
	if (dt <= 0.0)
		return current;
	if (smooth_time <= 0.0) {
		velocity = 0.0f;
		return target;
	}

	// Closed-form critically damped spring (Game Programming Gems 4, ch. 1.10), as used by Unity's SmoothDamp.
	const double omega = 2.0 / smooth_time;
	const double x = omega * dt;
	const double decay = 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x);

	double change = static_cast<double>(current) - target;
	if (max_speed > 0.0f) {
		const double max_change = static_cast<double>(max_speed) * smooth_time;
		change = std::clamp(change, -max_change, max_change);
	}
	const double limited_target = current - change;

	const double temp = (velocity + omega * change) * dt;
	velocity = static_cast<float>((velocity - omega * temp) * decay);
	double output = limited_target + (change + temp) * decay;

	// Never overshoot the real target.
	if ((target - current > 0.0f) == (output > target)) {
		output = target;
		velocity = 0.0f;
	}
	return static_cast<float>(output);
}

void CameraMotion::reset(const Rect &crop)
{
	const Vec2 center = crop.center();
	x_ = {center.x, 0.0f};
	y_ = {center.y, 0.0f};
	width_ = {crop.width, 0.0f};
	aim_x_ = {center.x, center.x, false};
	aim_y_ = {center.y, center.y, false};
	goal_width_ = crop.width;
	initialized_ = true;
}

void CameraMotion::update_aim(Aim &aim, float desired, float target, float current, float zone) const
{
	if (!aim.moving && std::abs(desired - aim.anchor) > zone)
		aim.moving = true;

	if (aim.moving) {
		aim.anchor = desired;
		aim.goal = target;
		if (std::abs(target - current) <= zone * kSettleFraction)
			aim.moving = false;
	}
}

Rect CameraMotion::update(const Rect &target, Vec2 desired_center, double dt)
{
	if (!initialized_ || target.width <= 0.0f || target.height <= 0.0f) {
		reset(target);
		return target;
	}

	const float aspect = target.width / target.height;
	const Vec2 center = target.center();
	const float zone_x = config_.dead_zone * width_.value;
	const float zone_y = config_.dead_zone * width_.value / aspect;
	update_aim(aim_x_, desired_center.x, center.x, x_.value, zone_x);
	update_aim(aim_y_, desired_center.y, center.y, y_.value, zone_y);

	if (std::abs(target.width - goal_width_) > config_.zoom_stability * goal_width_)
		goal_width_ = target.width;

	const float max_speed = config_.max_pan_speed * width_.value;
	x_.value = smooth_damp(x_.value, aim_x_.goal, x_.velocity, config_.position_smooth_s, max_speed, dt);
	y_.value = smooth_damp(y_.value, aim_y_.goal, y_.velocity, config_.position_smooth_s, max_speed, dt);
	width_.value = smooth_damp(width_.value, goal_width_, width_.velocity, config_.zoom_smooth_s, 0.0f, dt);

	return Rect::from_center({x_.value, y_.value}, width_.value, width_.value / aspect);
}

} // namespace autoframe::framing
