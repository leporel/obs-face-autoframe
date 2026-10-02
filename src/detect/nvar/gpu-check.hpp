#pragma once

#include <string>

namespace autoframe::nvar {

struct RendererGpu {
	bool is_d3d11 = false;
	bool is_nvidia = false;
	std::string adapter_name;
};

// Describes the adapter OBS renders on. Must be called inside the OBS graphics context.
// NvCVImage D3D11 interop only works when OBS renders on the NVIDIA GPU (matters on hybrid iGPU + dGPU systems).
[[nodiscard]] RendererGpu query_renderer_gpu();

} // namespace autoframe::nvar
