// Turns per-frame detections into stable tracks with persistent ids ("face numbers").
#pragma once

#include "framing/types.hpp"

#include <span>
#include <vector>

namespace autoframe::framing {

struct TrackerConfig {
	// Detections overlapping a track at least this much (IoU) continue it.
	float min_iou = 0.1f;
	// Without overlap, a detection still continues a track if its center moved at most this many track widths.
	float max_jump = 1.0f;
	// Consecutive detections needed before a track is trusted; filters one-off false positives.
	uint32_t min_hits = 2;
	// A confirmed track survives this long without detections.
	double lost_timeout_s = 1.0;
	// Weight of the newest velocity sample (0..1]; lower values give steadier velocity.
	float velocity_smoothing = 0.5f;
};

struct Track {
	uint32_t id = 0;
	Rect box;
	// Center velocity in pixels per second.
	Vec2 velocity;
	float confidence = 0.0f;
	uint32_t hits = 0;
	bool confirmed = false;
	double last_seen_s = 0.0;
	// Time since the last matching detection.
	double missed_s = 0.0;
};

class Tracker {
public:
	void configure(const TrackerConfig &config) { config_ = config; }

	// Matches a detection result to the existing tracks. `now_s` is the time of the analyzed frame.
	void update(std::span<const Observation> observations, double now_s);

	// Ages tracks and drops the ones lost for longer than the timeout. Call once per video frame.
	void advance(double now_s);

	void reset();

	[[nodiscard]] std::span<const Track> tracks() const { return tracks_; }

private:
	struct Candidate {
		float score;
		uint32_t track;
		uint32_t observation;
	};

	void collect_candidates(std::span<const Observation> observations);
	void update_track(Track &track, const Observation &observation, double now_s) const;

	TrackerConfig config_;
	std::vector<Track> tracks_;
	uint32_t next_id_ = 1;

	// Scratch buffers reused across updates.
	std::vector<Candidate> candidates_;
	std::vector<bool> track_matched_;
	std::vector<bool> observation_matched_;
};

} // namespace autoframe::framing
