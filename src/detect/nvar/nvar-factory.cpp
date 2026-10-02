#include "detect/nvar/nvar-factory.hpp"

#include "detect/async-detector.hpp"
#include "detect/nvar/gpu-check.hpp"
#include "detect/nvar/nvar-detector.hpp"
#include "util/localize.hpp"
#include "util/log.hpp"

namespace autoframe::nvar {

std::unique_ptr<IDetector> create_detector(DetectTarget target, std::string &error)
{
	const Api *api = runtime();
	if (!api) {
		error = localized("Status.SdkUnavailable", runtime_error());
		return nullptr;
	}

	const RendererGpu gpu = query_renderer_gpu();
	if (!gpu.is_d3d11) {
		error = localized("Status.NeedD3D11");
		return nullptr;
	}
	if (!gpu.is_nvidia) {
		error = localized("Status.NeedNvidiaGpu", gpu.adapter_name);
		return nullptr;
	}

	auto backend = std::make_unique<BoxDetector>(*api);
	std::string detail;
	if (!backend->init(target, detail)) {
		obs_log(LOG_WARNING, "detector init failed: %s", detail.c_str());
		error = localized("Status.DetectorFailed", detail);
		return nullptr;
	}

	return std::make_unique<AsyncDetector>(std::move(backend));
}

} // namespace autoframe::nvar
