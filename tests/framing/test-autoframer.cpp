#include "framing/autoframer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

using namespace autoframe::framing;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kFrame = 1.0 / 60.0;
// Detection every 3rd frame, as with "detect every N frames" = 3.
constexpr int kDetectEvery = 3;

AutoframerConfig config_full_hd()
{
	AutoframerConfig config;
	config.framer.bounds = Rect{0, 0, 1920, 1080};
	config.framer.aspect = 16.0f / 9.0f;
	config.framer.offset_y = 0.0f;
	config.prediction_s = 0.0;
	config.hold_time_s = 1.0;
	return config;
}

bool inside(const Rect &inner, const Rect &outer)
{
	constexpr float kEpsilon = 1e-3f;
	return inner.x >= outer.x - kEpsilon && inner.y >= outer.y - kEpsilon &&
	       inner.right() <= outer.right() + kEpsilon && inner.bottom() <= outer.bottom() + kEpsilon;
}

// Runs `frames` video frames with an optional face, detecting every few frames.
double run(Autoframer &framer, double now, int frames, const std::vector<Observation> &faces)
{
	for (int i = 0; i < frames; ++i) {
		now += kFrame;
		if (i % kDetectEvery == 0)
			framer.on_detections(faces, now);
		framer.update(now);
	}
	return now;
}

} // namespace

TEST_CASE("starts wide and glides onto a face", "[autoframer]")
{
	Autoframer framer;
	framer.configure(config_full_hd());

	const Rect face{1300, 300, 150, 150};
	double now = run(framer, 0.0, 1, {{face, 0.9f}});
	CHECK_THAT(framer.state().crop.width, WithinAbs(1920.0, 1.0));

	now = run(framer, now, 600, {{face, 0.9f}});
	const Rect crop = framer.state().crop;
	CHECK(crop.width < 1000.0f);
	CHECK(inside(face, crop));
	CHECK(inside(crop, Rect{0, 0, 1920, 1080}));
}

TEST_CASE("holds the framing briefly, then zooms out when nobody is found", "[autoframer]")
{
	Autoframer framer;
	framer.configure(config_full_hd());
	const Rect face{800, 400, 150, 150};
	double now = run(framer, 0.0, 600, {{face, 0.9f}});
	const float framed_width = framer.state().crop.width;

	// Lost for 0.5 s: still holding (hold_time 1 s).
	now = run(framer, now, 30, {});
	CHECK_THAT(framer.state().crop.width, WithinAbs(framed_width, 5.0));

	// Lost for a long time: back to the full frame.
	run(framer, now, 600, {});
	CHECK_THAT(framer.state().crop.width, WithinAbs(1920.0, 1.0));
}

TEST_CASE("crop stays inside a custom frame area", "[autoframer]")
{
	AutoframerConfig config = config_full_hd();
	config.framer.bounds = Rect{480, 0, 960, 1080};
	Autoframer framer;
	framer.configure(config);

	// The face is at the very edge of the area.
	run(framer, 0.0, 600, {{Rect{1350, 100, 120, 120}, 0.9f}});
	CHECK(inside(framer.state().crop, config.framer.bounds));
}

TEST_CASE("a long pause does not make the camera jump", "[autoframer]")
{
	Autoframer framer;
	framer.configure(config_full_hd());
	double now = run(framer, 0.0, 600, {{Rect{300, 300, 150, 150}, 0.9f}});
	const Rect before = framer.state().crop;

	// The source was hidden for 30 s; on return the face is on the other side.
	now += 30.0;
	framer.reset(now);
	framer.on_detections(std::vector<Observation>{{Rect{1500, 300, 150, 150}, 0.9f}}, now);
	framer.update(now + kFrame);

	const Rect after = framer.state().crop;
	CHECK(std::abs(after.center().x - before.center().x) < 100.0f);
}

TEST_CASE("reset keeps the framing for the hold time", "[autoframer]")
{
	Autoframer framer;
	framer.configure(config_full_hd());
	const double now = run(framer, 0.0, 600, {{Rect{800, 400, 150, 150}, 0.9f}});
	const float framed_width = framer.state().crop.width;

	framer.reset(now);
	run(framer, now, 20, {}); // 0.33 s without anyone, hold_time is 1 s
	CHECK_THAT(framer.state().crop.width, WithinAbs(framed_width, 5.0));
}
