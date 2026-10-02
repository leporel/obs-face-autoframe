#include "framing/motion.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace autoframe::framing;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kWidth = 960.0f;
constexpr float kHeight = 540.0f;

Rect crop_at(float cx, float cy, float width = kWidth)
{
	return Rect::from_center({cx, cy}, width, width * kHeight / kWidth);
}

// Runs the camera for `seconds` at `fps` towards a fixed target and returns the final crop.
Rect simulate(CameraMotion &motion, const Rect &target, double seconds, double fps)
{
	Rect crop{};
	const int steps = static_cast<int>(std::lround(seconds * fps));
	for (int i = 0; i < steps; ++i)
		crop = motion.update(target, 1.0 / fps);
	return crop;
}

MotionConfig no_dead_zone()
{
	MotionConfig config;
	config.dead_zone = 0.0f;
	config.zoom_stability = 0.0f;
	return config;
}

} // namespace

TEST_CASE("smooth_damp converges without overshooting", "[motion]")
{
	float value = 0.0f;
	float velocity = 0.0f;
	float previous = value;
	for (int i = 0; i < 600; ++i) {
		value = smooth_damp(value, 100.0f, velocity, 0.5, 0.0f, 1.0 / 60.0);
		REQUIRE(value <= 100.0f);
		REQUIRE(value >= previous); // monotonic
		previous = value;
	}
	CHECK_THAT(value, WithinAbs(100.0, 1e-2));
}

TEST_CASE("smooth_damp with zero smoothing snaps", "[motion]")
{
	float velocity = 0.0f;
	CHECK(smooth_damp(0.0f, 50.0f, velocity, 0.0, 0.0f, 1.0 / 60.0) == 50.0f);
}

TEST_CASE("camera motion does not depend on the frame rate", "[motion]")
{
	const Rect start = crop_at(480, 270);
	const Rect target = crop_at(1440, 270);

	Rect results[3];
	const double rates[3] = {30.0, 60.0, 144.0};
	for (int i = 0; i < 3; ++i) {
		CameraMotion motion;
		motion.configure(no_dead_zone());
		motion.reset(start);
		results[i] = simulate(motion, target, 0.5, rates[i]);
	}

	// Half-way through a 960 px move the positions must agree within a few pixels.
	CHECK_THAT(results[0].center().x, WithinAbs(results[2].center().x, 6.0));
	CHECK_THAT(results[1].center().x, WithinAbs(results[2].center().x, 3.0));
}

TEST_CASE("small movements inside the dead zone do not move the camera", "[motion]")
{
	MotionConfig config;
	config.dead_zone = 0.1f; // 96 px for a 960 px crop
	CameraMotion motion;
	motion.configure(config);
	motion.reset(crop_at(960, 540));

	const Rect crop = simulate(motion, crop_at(1010, 560), 2.0, 60.0);
	CHECK_THAT(crop.center().x, WithinAbs(960.0, 1e-3));
	CHECK_THAT(crop.center().y, WithinAbs(540.0, 1e-3));
}

TEST_CASE("leaving the dead zone re-centers the camera on the target", "[motion]")
{
	MotionConfig config;
	config.dead_zone = 0.1f;
	CameraMotion motion;
	motion.configure(config);
	motion.reset(crop_at(960, 540));

	const Rect crop = simulate(motion, crop_at(1300, 540), 5.0, 60.0);
	CHECK_THAT(crop.center().x, WithinAbs(1300.0, 5.0));
}

TEST_CASE("pan speed limit is respected", "[motion]")
{
	MotionConfig config = no_dead_zone();
	config.position_smooth_s = 0.1;
	config.max_pan_speed = 0.5f; // half a crop width (480 px) per second
	CameraMotion motion;
	motion.configure(config);
	motion.reset(crop_at(480, 540));

	const Rect crop = simulate(motion, crop_at(5000, 540), 1.0, 60.0);
	CHECK(crop.center().x - 480.0f <= 480.0f * 1.05f);
}

TEST_CASE("small size changes do not zoom", "[motion]")
{
	MotionConfig config;
	config.zoom_stability = 0.1f;
	CameraMotion motion;
	motion.configure(config);
	motion.reset(crop_at(960, 540, 960));

	const Rect crop = simulate(motion, crop_at(960, 540, 1000), 3.0, 60.0); // +4 %
	CHECK_THAT(crop.width, WithinAbs(960.0, 1e-3));
}

TEST_CASE("a target pinned to the frame-area edge is reached despite the dead zone", "[motion]")
{
	MotionConfig config;
	config.dead_zone = 0.1f; // 96 px for a 960 px crop
	CameraMotion motion;
	motion.configure(config);
	motion.reset(crop_at(600, 540));

	// The person moved far left (desired center 200), but the frame area only lets the camera reach 550:
	// the reachable step (50 px) is smaller than the dead zone, the person's movement (400 px) is not.
	Rect crop{};
	for (int i = 0; i < 300; ++i)
		crop = motion.update(crop_at(550, 540), Vec2{200, 540}, 1.0 / 60.0);
	CHECK_THAT(crop.center().x, WithinAbs(550.0, 1.0));
}
