// Detector contract used by the filter. Implementations must never block the OBS graphics thread on inference.
#pragma once

#include "detect/detection.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct gs_texture;

namespace autoframe {

struct DetectionResult {
	// Increments with every completed inference; 0 means "no result yet".
	uint64_t seq = 0;
	// Boxes normalized to the submitted frame.
	std::vector<Detection> objects;
	uint64_t inference_ns = 0;
};

enum class SubmitResult {
	Submitted,
	Busy,   // previous frame is still being processed; this frame is skipped
	Failed, // detector stopped working, see error()
};

class IDetector {
public:
	virtual ~IDetector() = default;

	// Graphics thread. Starts detection on an SDR BGRA frame unless the previous one is still running.
	virtual SubmitResult submit(gs_texture *bgra_frame) = 0;

	// Any thread. Copies the latest result into `out` if it is newer than out.seq; returns true when updated.
	virtual bool poll(DetectionResult &out) = 0;

	// Reason the detector stopped; empty while it works.
	[[nodiscard]] virtual std::string error() const = 0;
};

} // namespace autoframe
