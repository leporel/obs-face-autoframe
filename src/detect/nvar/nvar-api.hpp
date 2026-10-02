// Runtime loader for the NVIDIA AR SDK redistributable (nvARPose.dll + NVCVImage.dll).
#pragma once

#include "detect/nvar/nvar-defs.hpp"

#include <memory>
#include <string>

#include <Windows.h>

namespace autoframe::nvar {

class Api {
public:
	// Loads both SDK libraries and resolves every used symbol. On failure returns nullptr and fills `error`.
	[[nodiscard]] static std::unique_ptr<Api> load(std::string &error);

	~Api();
	Api(const Api &) = delete;
	Api &operator=(const Api &) = delete;

	[[nodiscard]] const char *error_string(NvCV_Status status) const;
	[[nodiscard]] const std::string &model_dir() const { return model_dir_; }
	[[nodiscard]] const std::string &version() const { return version_; }

	NvAR_Create_t NvAR_Create = nullptr;
	NvAR_Load_t NvAR_Load = nullptr;
	NvAR_Run_t NvAR_Run = nullptr;
	NvAR_Destroy_t NvAR_Destroy = nullptr;
	NvAR_CudaStreamCreate_t NvAR_CudaStreamCreate = nullptr;
	NvAR_CudaStreamDestroy_t NvAR_CudaStreamDestroy = nullptr;
	NvAR_SetString_t NvAR_SetString = nullptr;
	NvAR_SetCudaStream_t NvAR_SetCudaStream = nullptr;
	NvAR_SetObject_t NvAR_SetObject = nullptr;
	NvAR_SetF32Array_t NvAR_SetF32Array = nullptr;

	NvCV_GetErrorStringFromCode_t NvCV_GetErrorStringFromCode = nullptr;
	NvCVImage_Alloc_t NvCVImage_Alloc = nullptr;
	NvCVImage_Dealloc_t NvCVImage_Dealloc = nullptr;
	NvCVImage_InitFromD3D11Texture_t NvCVImage_InitFromD3D11Texture = nullptr;
	NvCVImage_MapResource_t NvCVImage_MapResource = nullptr;
	NvCVImage_UnmapResource_t NvCVImage_UnmapResource = nullptr;
	NvCVImage_Transfer_t NvCVImage_Transfer = nullptr;

private:
	Api() = default;
	bool load_libraries(std::string &error);
	bool resolve_symbols(std::string &error);

	HMODULE ar_module_ = nullptr;
	HMODULE cv_module_ = nullptr;
	std::wstring sdk_dir_;
	std::string model_dir_;
	std::string version_;
};

// Process-wide SDK instance, owned by the module (loaded in obs_module_load, released in obs_module_unload).
bool init_runtime();
void shutdown_runtime();
// nullptr when the SDK is unavailable; runtime_error() then explains why.
[[nodiscard]] const Api *runtime();
[[nodiscard]] const std::string &runtime_error();

} // namespace autoframe::nvar
