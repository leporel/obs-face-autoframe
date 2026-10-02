#include "framing/target-selector.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace autoframe::framing;

namespace {

Track person(uint32_t id, float x, float size, double missed = 0.0)
{
	Track t;
	t.id = id;
	t.box = Rect{x, 100, size, size};
	t.confirmed = true;
	t.hits = 5;
	t.missed_s = missed;
	return t;
}

constexpr Vec2 kCenter{960, 540};

} // namespace

TEST_CASE("locked mode ignores a second, larger person", "[selector]")
{
	SelectorConfig config;
	config.mode = FramingMode::SoloLocked;
	TargetSelector selector;
	selector.configure(config);

	std::vector<Track> tracks{person(1, 100, 100)};
	REQUIRE(selector.select(tracks, kCenter, 0.0));

	tracks.push_back(person(2, 800, 300));
	for (double t = 0.1; t < 5.0; t += 0.1) {
		const auto subject = selector.select(tracks, kCenter, t);
		REQUIRE(subject);
		CHECK(subject->box.x == 100);
	}
	CHECK(selector.selected_ids()[0] == 1);
}

TEST_CASE("locked mode picks someone else after the release time", "[selector]")
{
	SelectorConfig config;
	config.mode = FramingMode::SoloLocked;
	config.lock_release_s = 2.0;
	TargetSelector selector;
	selector.configure(config);

	std::vector<Track> tracks{person(1, 100, 100), person(2, 800, 100)};
	REQUIRE(selector.select(tracks, kCenter, 0.0));
	const uint32_t locked = selector.selected_ids()[0];

	// The locked person disappears; the other one stays ignored until the release time passes.
	std::erase_if(tracks, [locked](const Track &t) { return t.id == locked; });
	CHECK_FALSE(selector.select(tracks, kCenter, 1.0));
	REQUIRE(selector.select(tracks, kCenter, 2.5));
	CHECK(selector.selected_ids()[0] != locked);
}

TEST_CASE("closest-person mode switches only to a clearly larger face, after a delay", "[selector]")
{
	SelectorConfig config;
	config.switch_margin = 0.25f;
	config.switch_delay_s = 1.0;
	TargetSelector selector;
	selector.configure(config);

	std::vector<Track> tracks{person(1, 100, 100), person(2, 800, 105)};
	selector.select(tracks, kCenter, 0.0);
	const uint32_t first = selector.selected_ids()[0];
	REQUIRE(first == 2); // largest at start

	// Person 1 becomes slightly larger: below the margin, no switch.
	tracks[0].box.width = tracks[0].box.height = 110;
	selector.select(tracks, kCenter, 0.5);
	CHECK(selector.selected_ids()[0] == 2);

	// Person 1 becomes clearly larger: switch only after the delay.
	tracks[0].box.width = tracks[0].box.height = 200;
	selector.select(tracks, kCenter, 1.0);
	CHECK(selector.selected_ids()[0] == 2);
	selector.select(tracks, kCenter, 2.1);
	CHECK(selector.selected_ids()[0] == 1);
}

TEST_CASE("group mode frames everyone, limited mode only the largest", "[selector]")
{
	std::vector<Track> tracks{person(1, 100, 100), person(2, 800, 150), person(3, 1500, 120)};

	SelectorConfig config;
	config.mode = FramingMode::Group;
	TargetSelector selector;
	selector.configure(config);
	auto subject = selector.select(tracks, kCenter, 0.0);
	REQUIRE(subject);
	CHECK(subject->box.x == 100);
	CHECK(subject->box.right() == 1620);

	config.mode = FramingMode::GroupLimited;
	config.max_group = 2;
	selector.configure(config);
	subject = selector.select(tracks, kCenter, 0.0);
	REQUIRE(subject);
	CHECK(selector.selected_ids().size() == 2);
	CHECK(subject->box.x == 800); // person 1 (smallest) is left out
}

TEST_CASE("unconfirmed tracks are never framed", "[selector]")
{
	Track t = person(1, 100, 100);
	t.confirmed = false;
	std::vector<Track> tracks{t};

	TargetSelector selector;
	CHECK_FALSE(selector.select(tracks, kCenter, 0.0));
}
