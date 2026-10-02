#include "detect/nvar/nvar-detector.hpp"

#include "util/log.hpp"

#include <algorithm>

namespace autoframe::nvar {
namespace {

const char *feature_id(DetectTarget target)
{
	return target == DetectTarget::Body ? kFeatureBodyDetection : kFeatureFaceBoxDetection;
}

std::string describe(const Api &api, const char *what, NvCV_Status status)
{
	return std::string(what) + " failed: " + api.error_string(status) + " (" + std::to_string(status) + ")";
}

} // namespace

BoxDetector::BoxDetector(const Api &api) : api_(api), frame_(api), bgr_(api), staging_(api)
{
	bboxes_.boxes = boxes_.data();
	bboxes_.max_boxes = static_cast<uint8_t>(boxes_.size());
}

BoxDetector::~BoxDetector()
{
	release();
}

void BoxDetector::release()
{
	if (feature_) {
		if (const NvCV_Status status = api_.NvAR_Destroy(feature_); status != NVCV_SUCCESS)
			obs_log(LOG_WARNING, "%s", describe(api_, "NvAR_Destroy", status).c_str());
		feature_ = nullptr;
	}

	frame_.reset();
	bgr_.reset();
	staging_.reset();

	if (stream_) {
		if (const NvCV_Status status = api_.NvAR_CudaStreamDestroy(stream_); status != NVCV_SUCCESS)
			obs_log(LOG_WARNING, "%s", describe(api_, "NvAR_CudaStreamDestroy", status).c_str());
		stream_ = nullptr;
	}
	needs_load_ = true;
}

bool BoxDetector::check(NvCV_Status status, const char *what, std::string &error) const
{
	if (status == NVCV_SUCCESS)
		return true;
	error = describe(api_, what, status);
	return false;
}

bool BoxDetector::init(DetectTarget target, std::string &error)
{
	release();
	target_ = target;

	if (!check(api_.NvAR_CudaStreamCreate(&stream_), "NvAR_CudaStreamCreate", error))
		return false;

	if (!check(api_.NvAR_Create(feature_id(target_), &feature_), "NvAR_Create", error))
		return false;

	return configure_feature(error);
}

bool BoxDetector::configure_feature(std::string &error)
{
	// Box detection features take TRTModelDir in recent SDKs and ModelDir in older ones; an unknown selector is
	// reported as NVCV_ERR_SELECTOR, so both are tried and at least one must be accepted.
	const std::string &model_dir = api_.model_dir();
	if (!model_dir.empty()) {
		const NvCV_Status model = api_.NvAR_SetString(feature_, NVAR_PARAM_CONFIG(ModelDir), model_dir.c_str());
		const NvCV_Status trt =
			api_.NvAR_SetString(feature_, NVAR_PARAM_CONFIG(TRTModelDir), model_dir.c_str());
		if (model != NVCV_SUCCESS && trt != NVCV_SUCCESS)
			return check(model, "set model directory", error);
	}

	if (!check(api_.NvAR_SetCudaStream(feature_, NVAR_PARAM_CONFIG(CUDAStream), stream_), "set CUDAStream",
		   error))
		return false;

	if (!check(api_.NvAR_SetObject(feature_, NVAR_PARAM_OUTPUT(BoundingBoxes), &bboxes_, sizeof(NvAR_BBoxes)),
		   "set BoundingBoxes", error))
		return false;

	return check(api_.NvAR_SetF32Array(feature_, NVAR_PARAM_OUTPUT(BoundingBoxesConfidence), confidences_.data(),
					   static_cast<int>(confidences_.size())),
		     "set BoundingBoxesConfidence", error);
}

bool BoxDetector::prepare_input(unsigned width, unsigned height, std::string &error)
{
	if (!bgr_.empty() && bgr_.width() == width && bgr_.height() == height)
		return true;

	needs_load_ = true;
	const bool ok =
		check(bgr_.alloc_gpu(width, height, NVCV_BGR, NVCV_CHUNKY), "allocate BGR image", error) &&
		check(staging_.alloc_gpu(width, height, NVCV_RGBA, NVCV_PLANAR), "allocate staging image", error) &&
		check(api_.NvAR_SetObject(feature_, NVAR_PARAM_INPUT(Image), bgr_.get(), sizeof(NvCVImage)),
		      "set input Image", error);
	if (!ok)
		bgr_.reset(); // forces a full retry on the next upload
	return ok;
}

bool BoxDetector::upload(gs_texture *bgra_frame, std::string &error)
{
	if (!feature_) {
		error = "detector is not initialized";
		return false;
	}

	auto *texture = static_cast<ID3D11Texture2D *>(gs_texture_get_obj(bgra_frame));
	if (!texture) {
		error = "detection frame has no D3D11 texture";
		return false;
	}

	// gs_texrender recreates its texture on resize, so the CUDA registration must follow the texture object.
	if (frame_.wrapped_texture() != texture &&
	    !check(frame_.wrap_d3d11(texture), "NvCVImage_InitFromD3D11Texture", error))
		return false;

	if (!prepare_input(frame_.width(), frame_.height(), error))
		return false;

	// Map/transfer/unmap only enqueue work on the CUDA stream; inference on the same stream runs after it.
	MappedImage mapped(api_, frame_, stream_);
	if (!check(mapped.status(), "NvCVImage_MapResource", error))
		return false;

	return check(api_.NvCVImage_Transfer(frame_.get(), bgr_.get(), 1.0f, stream_, staging_.get()),
		     "NvCVImage_Transfer", error);
}

bool BoxDetector::infer(std::vector<Detection> &out, std::string &error)
{
	out.clear();
	if (bgr_.empty()) {
		error = "detector has no input";
		return false;
	}

	if (needs_load_) {
		// Builds/loads the TensorRT engine; only happens on start and on detection size changes.
		if (!check(api_.NvAR_Load(feature_), "NvAR_Load", error))
			return false;
		needs_load_ = false;
		obs_log(LOG_INFO, "%s detection loaded for %ux%u input", feature_id(target_), bgr_.width(),
			bgr_.height());
	}

	bboxes_.num_boxes = 0;
	if (!check(api_.NvAR_Run(feature_), "NvAR_Run", error))
		return false;

	const float inv_width = 1.0f / static_cast<float>(bgr_.width());
	const float inv_height = 1.0f / static_cast<float>(bgr_.height());
	const size_t count = std::min<size_t>(bboxes_.num_boxes, boxes_.size());
	for (size_t i = 0; i < count; ++i) {
		const NvAR_Rect &box = boxes_[i];
		out.push_back(Detection{
			.box = {box.x * inv_width, box.y * inv_height, box.width * inv_width, box.height * inv_height},
			.confidence = confidences_[i],
		});
	}
	return true;
}

} // namespace autoframe::nvar
