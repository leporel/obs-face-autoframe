#include "framing/tracker.hpp"

#include <algorithm>

namespace autoframe::framing {
namespace {

// Center proximity adds at most this much to the IoU score, so overlap dominates matching.
constexpr float kDistanceScoreWeight = 0.5f;

} // namespace

void Tracker::reset()
{
	tracks_.clear();
	next_id_ = 1;
}

void Tracker::collect_candidates(std::span<const Observation> observations)
{
	candidates_.clear();
	for (uint32_t t = 0; t < tracks_.size(); ++t) {
		const Rect &track_box = tracks_[t].box;
		const float jump_limit = config_.max_jump * std::max(track_box.width, track_box.height);

		for (uint32_t o = 0; o < observations.size(); ++o) {
			const Rect &box = observations[o].box;
			const float overlap = iou(track_box, box);
			const float distance = length(box.center() - track_box.center());
			const bool near = jump_limit > 0.0f && distance <= jump_limit;
			if (overlap < config_.min_iou && !near)
				continue;

			const float proximity = near ? 1.0f - distance / jump_limit : 0.0f;
			candidates_.push_back({overlap + proximity * kDistanceScoreWeight, t, o});
		}
	}
	std::sort(candidates_.begin(), candidates_.end(),
		  [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
}

void Tracker::update_track(Track &track, const Observation &observation, double now_s) const
{
	const double dt = now_s - track.last_seen_s;
	if (dt > 0.0) {
		const Vec2 sample = (observation.box.center() - track.box.center()) * static_cast<float>(1.0 / dt);
		// The first sample has nothing to smooth against.
		const float weight = track.hits <= 1 ? 1.0f : config_.velocity_smoothing;
		track.velocity = track.velocity + (sample - track.velocity) * weight;
	}

	track.box = observation.box;
	track.confidence = observation.confidence;
	track.last_seen_s = now_s;
	track.missed_s = 0.0;
	++track.hits;
	track.confirmed = track.confirmed || track.hits >= config_.min_hits;
}

void Tracker::update(std::span<const Observation> observations, double now_s)
{
	collect_candidates(observations);
	track_matched_.assign(tracks_.size(), false);
	observation_matched_.assign(observations.size(), false);

	// Greedy assignment, best pairs first.
	for (const Candidate &c : candidates_) {
		if (track_matched_[c.track] || observation_matched_[c.observation])
			continue;
		track_matched_[c.track] = true;
		observation_matched_[c.observation] = true;
		update_track(tracks_[c.track], observations[c.observation], now_s);
	}

	// Unconfirmed tracks must be seen in consecutive results, otherwise they were noise.
	size_t kept = 0;
	for (size_t i = 0; i < tracks_.size(); ++i) {
		if (!track_matched_[i] && !tracks_[i].confirmed)
			continue;
		if (kept != i)
			tracks_[kept] = tracks_[i];
		++kept;
	}
	tracks_.resize(kept);

	for (uint32_t o = 0; o < observations.size(); ++o) {
		if (observation_matched_[o])
			continue;
		Track track;
		track.id = next_id_++;
		track.box = observations[o].box;
		track.confidence = observations[o].confidence;
		track.hits = 1;
		track.confirmed = config_.min_hits <= 1;
		track.last_seen_s = now_s;
		tracks_.push_back(track);
	}
}

void Tracker::advance(double now_s)
{
	for (Track &track : tracks_)
		track.missed_s = std::max(0.0, now_s - track.last_seen_s);

	std::erase_if(tracks_, [this](const Track &track) { return track.missed_s > config_.lost_timeout_s; });
}

} // namespace autoframe::framing
