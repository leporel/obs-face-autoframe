#include "framing/tracker.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

using namespace autoframe::framing;
using Catch::Matchers::WithinAbs;

namespace {

Observation face(float x, float y, float size = 100.0f)
{
	return {Rect{x, y, size, size}, 0.9f};
}

void feed(Tracker &tracker, std::vector<Observation> observations, double now)
{
	tracker.advance(now);
	tracker.update(observations, now);
}

} // namespace

TEST_CASE("a moving face keeps its id", "[tracker]")
{
	Tracker tracker;
	feed(tracker, {face(100, 100)}, 0.0);
	const uint32_t id = tracker.tracks()[0].id;

	for (int i = 1; i <= 10; ++i)
		feed(tracker, {face(100.0f + i * 20.0f, 100)}, i * 0.1);

	REQUIRE(tracker.tracks().size() == 1);
	CHECK(tracker.tracks()[0].id == id);
	CHECK(tracker.tracks()[0].confirmed);
}

TEST_CASE("a second face gets a new id", "[tracker]")
{
	Tracker tracker;
	feed(tracker, {face(100, 100)}, 0.0);
	feed(tracker, {face(100, 100), face(1000, 100)}, 0.1);

	REQUIRE(tracker.tracks().size() == 2);
	CHECK(tracker.tracks()[0].id != tracker.tracks()[1].id);
}

TEST_CASE("a one-off detection is discarded", "[tracker]")
{
	Tracker tracker; // min_hits = 2
	feed(tracker, {face(100, 100)}, 0.0);
	feed(tracker, {}, 0.1);
	CHECK(tracker.tracks().empty());
}

TEST_CASE("a confirmed track survives short gaps and expires after the timeout", "[tracker]")
{
	TrackerConfig config;
	config.lost_timeout_s = 1.0;
	Tracker tracker;
	tracker.configure(config);
	feed(tracker, {face(100, 100)}, 0.0);
	feed(tracker, {face(100, 100)}, 0.1);

	tracker.advance(0.8);
	REQUIRE(tracker.tracks().size() == 1);
	CHECK_THAT(tracker.tracks()[0].missed_s, WithinAbs(0.7, 1e-9));

	tracker.advance(1.2);
	CHECK(tracker.tracks().empty());
}

TEST_CASE("velocity follows the movement", "[tracker]")
{
	TrackerConfig config;
	config.velocity_smoothing = 1.0f;
	Tracker tracker;
	tracker.configure(config);
	feed(tracker, {face(100, 100)}, 0.0);
	feed(tracker, {face(150, 100)}, 0.5); // 100 px/s to the right

	CHECK_THAT(tracker.tracks()[0].velocity.x, WithinAbs(100.0, 1e-3));
	CHECK_THAT(tracker.tracks()[0].velocity.y, WithinAbs(0.0, 1e-3));
}
