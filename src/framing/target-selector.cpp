#include "framing/target-selector.hpp"

#include <algorithm>
#include <limits>

namespace autoframe::framing {
namespace {

const Track *find_track(std::span<const Track> tracks, uint32_t id)
{
	if (id == 0)
		return nullptr;
	const auto it = std::find_if(tracks.begin(), tracks.end(), [id](const Track &t) { return t.id == id; });
	return it == tracks.end() ? nullptr : &*it;
}

const Track *largest_confirmed(std::span<const Track> tracks)
{
	const Track *best = nullptr;
	for (const Track &t : tracks) {
		if (t.confirmed && (!best || t.box.area() > best->box.area()))
			best = &t;
	}
	return best;
}

} // namespace

void TargetSelector::configure(const SelectorConfig &config)
{
	if (config.mode != config_.mode)
		reset();
	config_ = config;
}

void TargetSelector::reset()
{
	current_id_ = 0;
	challenger_id_ = 0;
	challenger_since_s_ = 0.0;
	current_last_seen_s_ = 0.0;
	last_center_.reset();
	selected_.clear();
}

std::optional<Subject> TargetSelector::select(std::span<const Track> tracks, Vec2 frame_center, double now_s)
{
	selected_.clear();
	switch (config_.mode) {
	case FramingMode::SoloLocked:
		return select_locked(tracks, frame_center, now_s);
	case FramingMode::Group:
		return select_group(tracks, std::numeric_limits<uint32_t>::max());
	case FramingMode::GroupLimited:
		return select_group(tracks, std::max<uint32_t>(config_.max_group, 1));
	case FramingMode::SoloLargest:
		break;
	}
	return select_largest(tracks, now_s);
}

Subject TargetSelector::solo(const Track &track)
{
	selected_.push_back(track.id);
	last_center_ = track.box.center();
	current_last_seen_s_ = track.last_seen_s;
	return {track.box, track.velocity, track.missed_s};
}

std::optional<Subject> TargetSelector::select_largest(std::span<const Track> tracks, double now_s)
{
	const Track *largest = largest_confirmed(tracks);
	if (!largest)
		return std::nullopt;

	const Track *current = find_track(tracks, current_id_);
	if (!current || !current->confirmed) {
		current_id_ = largest->id;
		challenger_id_ = 0;
		return solo(*largest);
	}

	// Switching needs a clearly larger person for a while, so two similar faces do not make the camera flip.
	const bool clearly_larger = largest != current &&
				    largest->box.area() > current->box.area() * (1.0f + config_.switch_margin);
	if (!clearly_larger) {
		challenger_id_ = 0;
	} else if (challenger_id_ != largest->id) {
		challenger_id_ = largest->id;
		challenger_since_s_ = now_s;
	} else if (now_s - challenger_since_s_ >= config_.switch_delay_s) {
		current_id_ = largest->id;
		challenger_id_ = 0;
		return solo(*largest);
	}
	return solo(*current);
}

std::optional<Subject> TargetSelector::select_locked(std::span<const Track> tracks, Vec2 frame_center, double now_s)
{
	const Track *current = find_track(tracks, current_id_);
	if (current && current->missed_s < config_.lock_release_s)
		return solo(*current);

	if (current_id_ != 0) {
		const double lost_for = current ? current->missed_s : now_s - current_last_seen_s_;
		if (lost_for < config_.lock_release_s)
			return std::nullopt; // keep waiting for the locked person; others stay ignored
	}

	const Track *next = pick(tracks, config_.relock, frame_center);
	current_id_ = next ? next->id : 0;
	return next ? std::optional<Subject>(solo(*next)) : std::nullopt;
}

const Track *TargetSelector::pick(std::span<const Track> tracks, RelockPolicy policy, Vec2 frame_center) const
{
	const bool by_last = policy == RelockPolicy::ClosestToLast && last_center_.has_value();
	const bool by_distance = policy == RelockPolicy::ClosestToCenter || by_last;
	const Vec2 anchor = by_last ? *last_center_ : frame_center;

	const Track *best = nullptr;
	float best_score = 0.0f;
	for (const Track &t : tracks) {
		// Only people currently visible are eligible; stale tracks are about to disappear.
		if (!t.confirmed || t.missed_s >= config_.lock_release_s)
			continue;
		const float score = by_distance ? -length(t.box.center() - anchor) : t.box.area();
		if (!best || score > best_score) {
			best = &t;
			best_score = score;
		}
	}
	return best;
}

std::optional<Subject> TargetSelector::select_group(std::span<const Track> tracks, uint32_t limit)
{
	scratch_.clear();
	for (const Track &t : tracks) {
		if (t.confirmed)
			scratch_.push_back(&t);
	}
	if (scratch_.empty())
		return std::nullopt;

	if (scratch_.size() > limit) {
		std::partial_sort(scratch_.begin(), scratch_.begin() + limit, scratch_.end(),
				  [](const Track *a, const Track *b) { return a->box.area() > b->box.area(); });
		scratch_.resize(limit);
	}

	Subject subject{scratch_.front()->box, {}, scratch_.front()->missed_s};
	Vec2 velocity_sum;
	for (const Track *t : scratch_) {
		subject.box = union_of(subject.box, t->box);
		velocity_sum = velocity_sum + t->velocity;
		subject.staleness_s = std::min(subject.staleness_s, t->missed_s);
		selected_.push_back(t->id);
	}
	subject.velocity = velocity_sum * (1.0f / static_cast<float>(scratch_.size()));
	last_center_ = subject.box.center();
	return subject;
}

} // namespace autoframe::framing
