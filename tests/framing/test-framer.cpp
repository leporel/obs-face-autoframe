#include "framing/framer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace autoframe::framing;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kAspect = 16.0f / 9.0f;

FramerConfig full_hd()
{
	FramerConfig config;
	config.padding_x = 0.5f;
	config.padding_y = 0.5f;
	config.offset_x = 0.0f;
	config.offset_y = 0.0f;
	config.min_zoom = 1.0f;
	config.max_zoom = 3.0f;
	config.aspect = kAspect;
	config.bounds = Rect{0, 0, 1920, 1080};
	return config;
}

bool inside(const Rect &inner, const Rect &outer)
{
	constexpr float kEpsilon = 1e-3f;
	return inner.x >= outer.x - kEpsilon && inner.y >= outer.y - kEpsilon &&
	       inner.right() <= outer.right() + kEpsilon && inner.bottom() <= outer.bottom() + kEpsilon;
}

} // namespace

TEST_CASE("crop has the output aspect and contains the padded subject", "[framer]")
{
	Framer framer;
	framer.configure(full_hd());
	const Rect subject{900, 400, 200, 200};
	const Rect crop = framer.frame(subject);

	CHECK_THAT(crop.width / crop.height, WithinAbs(kAspect, 1e-4));
	// Padded subject is 400x400.
	CHECK(crop.height >= 400.0f - 1e-3f);
	CHECK(inside(subject, crop));
	CHECK_THAT(crop.center().x, WithinAbs(1000.0, 1e-3));
}

TEST_CASE("max zoom limits how small the crop gets", "[framer]")
{
	Framer framer;
	framer.configure(full_hd());
	const Rect crop = framer.frame(Rect{950, 500, 10, 10});
	CHECK_THAT(crop.width, WithinAbs(1920.0 / 3.0, 1e-2));
}

TEST_CASE("min zoom limits how large the crop gets", "[framer]")
{
	FramerConfig config = full_hd();
	config.min_zoom = 2.0f;
	Framer framer;
	framer.configure(config);
	const Rect crop = framer.frame(Rect{0, 0, 1900, 1000});
	CHECK_THAT(crop.width, WithinAbs(960.0, 1e-2));
}

TEST_CASE("crop never leaves the bounds", "[framer]")
{
	FramerConfig config = full_hd();
	config.bounds = Rect{200, 100, 1200, 800};
	Framer framer;
	framer.configure(config);

	for (const Rect &subject : {Rect{0, 0, 100, 100}, Rect{1800, 1000, 100, 80}, Rect{1300, 50, 300, 300}}) {
		const Rect crop = framer.frame(subject);
		CHECK(inside(crop, config.bounds));
	}
}

TEST_CASE("idle crop is the widest output-aspect crop inside the bounds", "[framer]")
{
	FramerConfig config = full_hd();
	config.bounds = Rect{0, 0, 960, 1080}; // narrow area: the width limits the crop
	Framer framer;
	framer.configure(config);

	const Rect idle = framer.idle();
	CHECK_THAT(idle.width, WithinAbs(960.0, 1e-2));
	CHECK_THAT(idle.height, WithinAbs(540.0, 1e-2));
	CHECK(inside(idle, config.bounds));
}

TEST_CASE("offset moves the framing relative to the subject size", "[framer]")
{
	FramerConfig config = full_hd();
	config.offset_y = 0.25f;
	Framer framer;
	framer.configure(config);

	const Rect crop = framer.frame(Rect{900, 400, 200, 200});
	CHECK_THAT(crop.center().y, WithinAbs(500.0 + 50.0, 1e-3));
}
