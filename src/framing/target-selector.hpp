// Decides which tracks are framed: one person (largest or locked by id) or a group.
#pragma once

#include "framing/tracker.hpp"

#include <optional>
#include <span>
#include <vector>

namespace autoframe::framing {

enum class FramingMode : int {
	SoloLargest = 0,  // the closest (largest) person; switches only to a clearly larger one
	SoloLocked = 1,   // the first person found; everyone else is ignored until that person is gone
	Group = 2,        // everyone in the frame
	GroupLimited = 3, // the N largest people
};

enum class RelockPolicy : int {
	Largest = 0,
	ClosestToCenter = 1,
	ClosestToLast = 2, // closest to where the previous person was last seen
};

struct SelectorConfig {
	FramingMode mode = FramingMode::SoloLargest;
	uint32_t max_group = 2;
	// SoloLargest: another person must be this much larger (area ratio - 1) ...
	float switch_margin = 0.25f;
	// ... for this long before the camera switches to them.
	double switch_delay_s = 1.0;
	// SoloLocked: how long the locked person may be missing before another one is picked.
	double lock_release_s = 2.0;
	RelockPolicy relock = RelockPolicy::Largest;
};

// What the camera should frame.
struct Subject {
	Rect box;
	// Average center velocity of the selected tracks, pixels per second.
	Vec2 velocity;
	// Time since the freshest selected track was last detected.
	double staleness_s = 0.0;
};

class TargetSelector {
public:
	void configure(const SelectorConfig &config);

	// `frame_center` is used by the ClosestToCenter policy.
	[[nodiscard]] std::optional<Subject> select(std::span<const Track> tracks, Vec2 frame_center, double now_s);

	// Forgets the locked / current person; the next call picks again.
	void reset();

	[[nodiscard]] std::span<const uint32_t> selected_ids() const { return selected_; }

private:
	std::optional<Subject> select_largest(std::span<const Track> tracks, double now_s);
	std::optional<Subject> select_locked(std::span<const Track> tracks, Vec2 frame_center, double now_s);
	std::optional<Subject> select_group(std::span<const Track> tracks, uint32_t limit);
	const Track *pick(std::span<const Track> tracks, RelockPolicy policy, Vec2 frame_center) const;
	Subject solo(const Track &track);

	SelectorConfig config_;
	uint32_t current_id_ = 0;
	uint32_t challenger_id_ = 0;
	double challenger_since_s_ = 0.0;
	double current_last_seen_s_ = 0.0;
	std::optional<Vec2> last_center_;

	std::vector<uint32_t> selected_;
	std::vector<const Track *> scratch_;
};

} // namespace autoframe::framing
