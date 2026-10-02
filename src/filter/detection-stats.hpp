// Detection throughput counters, reported periodically while Debug view is enabled.
#pragma once

#include <cstdint>

namespace autoframe {

class DetectionStats {
public:
	struct Summary {
		double seconds;
		uint32_t submitted;
		uint32_t skipped_busy;
		uint32_t completed;
		double avg_inference_ms;
	};

	void on_submitted() { ++submitted_; }
	void on_skipped_busy() { ++skipped_busy_; }
	void on_completed(uint64_t inference_ns)
	{
		++completed_;
		inference_ns_total_ += inference_ns;
	}

	// Returns true and fills `out` once per `period_ns`, then starts a new window.
	bool take_summary(uint64_t now_ns, uint64_t period_ns, Summary &out)
	{
		if (window_start_ns_ == 0) {
			window_start_ns_ = now_ns;
			return false;
		}
		if (now_ns - window_start_ns_ < period_ns)
			return false;

		constexpr double kNsPerSecond = 1e9;
		constexpr double kNsPerMs = 1e6;
		out.seconds = static_cast<double>(now_ns - window_start_ns_) / kNsPerSecond;
		out.submitted = submitted_;
		out.skipped_busy = skipped_busy_;
		out.completed = completed_;
		out.avg_inference_ms =
			completed_ ? static_cast<double>(inference_ns_total_) / completed_ / kNsPerMs : 0.0;

		*this = DetectionStats{};
		window_start_ns_ = now_ns;
		return true;
	}

private:
	uint64_t window_start_ns_ = 0;
	uint32_t submitted_ = 0;
	uint32_t skipped_busy_ = 0;
	uint32_t completed_ = 0;
	uint64_t inference_ns_total_ = 0;
};

} // namespace autoframe
