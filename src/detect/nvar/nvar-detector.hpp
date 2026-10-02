// Face / body box detection on top of the NVIDIA AR SDK.
#pragma once

#include "detect/detection-backend.hpp"
#include "detect/nvar/nvcv-image.hpp"

#include <array>
#include <string>
#include <vector>

namespace autoframe::nvar {

class BoxDetector final : public DetectionBackend {
public:
	explicit BoxDetector(const Api &api);
	~BoxDetector() override;
	BoxDetector(const BoxDetector &) = delete;
	BoxDetector &operator=(const BoxDetector &) = delete;

	// Creates the CUDA stream and the SDK feature. Must be called inside the OBS graphics context.
	bool init(DetectTarget target, std::string &error);

	// Graphics thread: converts a BGRA unorm texture into the SDK input buffer (GPU only, no CPU readback).
	bool upload(gs_texture *bgra_frame, std::string &error) override;

	// Worker thread: (re)loads the model after an input size change, then runs inference.
	bool infer(std::vector<Detection> &out, std::string &error) override;

private:
	bool check(NvCV_Status status, const char *what, std::string &error) const;
	bool configure_feature(std::string &error);
	bool prepare_input(unsigned width, unsigned height, std::string &error);
	void release();

	const Api &api_;
	DetectTarget target_ = DetectTarget::Face;
	CUstream stream_ = nullptr;
	FeatureHandle feature_ = nullptr;

	Image frame_;   // view of the OBS texture
	Image bgr_;     // SDK input image
	Image staging_; // scratch buffer for format conversion

	std::array<NvAR_Rect, kMaxDetections> boxes_{};
	std::array<float, kMaxDetections> confidences_{};
	NvAR_BBoxes bboxes_{};
	// Set on the graphics thread when the input image changes; NvAR_Load then runs on the worker.
	bool needs_load_ = true;
};

} // namespace autoframe::nvar
