// Synchronous inference engine driven by AsyncDetector. One implementation per detection SDK.
#pragma once

#include "detect/detection.hpp"

#include <string>
#include <vector>

struct gs_texture;

namespace autoframe {

class DetectionBackend {
public:
	virtual ~DetectionBackend() = default;

	// Graphics thread; never called while infer() runs. Copies the frame into the engine input (GPU only).
	virtual bool upload(gs_texture *bgra_frame, std::string &error) = 0;

	// Worker thread. Runs inference on the last uploaded frame; boxes are normalized to that frame.
	virtual bool infer(std::vector<Detection> &out, std::string &error) = 0;
};

} // namespace autoframe
