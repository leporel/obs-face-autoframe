#include "framing/types.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace autoframe::framing;
using Catch::Matchers::WithinAbs;

TEST_CASE("iou is 1 for identical and 0 for disjoint rectangles", "[types]")
{
	const Rect a{0, 0, 10, 10};
	CHECK_THAT(iou(a, a), WithinAbs(1.0, 1e-6));
	CHECK_THAT(iou(a, Rect{20, 20, 10, 10}), WithinAbs(0.0, 1e-6));
}

TEST_CASE("iou of half-overlapping rectangles", "[types]")
{
	// Overlap 5x10 = 50, union 150.
	CHECK_THAT(iou(Rect{0, 0, 10, 10}, Rect{5, 0, 10, 10}), WithinAbs(50.0 / 150.0, 1e-6));
}

TEST_CASE("union covers both rectangles", "[types]")
{
	const Rect u = union_of(Rect{0, 0, 10, 10}, Rect{20, 5, 10, 10});
	CHECK(u.x == 0);
	CHECK(u.y == 0);
	CHECK(u.right() == 30);
	CHECK(u.bottom() == 15);
}
