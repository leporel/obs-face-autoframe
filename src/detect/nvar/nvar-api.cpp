#include "detect/nvar/nvar-api.hpp"

#include "util/log.hpp"

#include <util/bmem.h>
#include <util/platform.h>
#include <util/windows/win-version.h>

#include <memory>

namespace autoframe::nvar {
namespace {

constexpr const wchar_t *kArLibrary = L"nvARPose.dll";
constexpr const wchar_t *kCvLibrary = L"NVCVImage.dll";
constexpr const wchar_t *kSdkPathEnv = L"NV_AR_SDK_PATH";
constexpr const wchar_t *kModelDirEnv = L"NVAR_MODEL_DIR";
// NVIDIA's documented value of NV_AR_SDK_PATH meaning "load the SDK DLLs with the standard search order".
constexpr const wchar_t *kUseAppPath = L"USE_APP_PATH";
constexpr const wchar_t *kDefaultSdkSubdir = L"\\NVIDIA Corporation\\NVIDIA AR SDK";

std::unique_ptr<Api> g_runtime;
std::string g_runtime_error;

std::wstring read_env(const wchar_t *name)
{
	const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
	if (size == 0)
		return {};

	std::wstring value(size, L'\0');
	const DWORD written = GetEnvironmentVariableW(name, value.data(), size);
	if (written == 0 || written >= size)
		return {};

	value.resize(written);
	return value;
}

std::string to_utf8(const std::wstring &text)
{
	char *utf8 = nullptr;
	if (os_wcs_to_utf8_ptr(text.c_str(), text.size(), &utf8) == 0 || !utf8)
		return {};

	std::string result(utf8);
	bfree(utf8);
	return result;
}

std::wstring resolve_sdk_dir()
{
	std::wstring dir = read_env(kSdkPathEnv);
	if (dir == kUseAppPath)
		return {};
	if (!dir.empty())
		return dir;

	const std::wstring program_files = read_env(L"ProgramFiles");
	return program_files.empty() ? std::wstring{} : program_files + kDefaultSdkSubdir;
}

std::wstring library_path(const std::wstring &sdk_dir, const wchar_t *library)
{
	return sdk_dir.empty() ? std::wstring(library) : sdk_dir + L"\\" + library;
}

HMODULE load_library(const std::wstring &path, bool has_dir)
{
	// LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR lets the SDK DLL find its CUDA/TensorRT dependencies next to it without
	// changing the process-wide DLL search path (which would affect OBS and other plugins).
	const DWORD flags = has_dir ? (LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) : 0;
	return LoadLibraryExW(path.c_str(), nullptr, flags);
}

std::string read_version(const std::wstring &path)
{
	win_version_info info{};
	if (!get_dll_ver(path.c_str(), &info))
		return "unknown";

	return std::to_string(info.major) + "." + std::to_string(info.minor) + "." + std::to_string(info.build) + "." +
	       std::to_string(info.revis);
}

template<typename Fn> bool resolve(HMODULE module, const char *name, Fn &out, std::string &error)
{
	out = reinterpret_cast<Fn>(GetProcAddress(module, name));
	if (!out) {
		error = std::string("missing SDK symbol ") + name;
		return false;
	}
	return true;
}

} // namespace

std::unique_ptr<Api> Api::load(std::string &error)
{
	std::unique_ptr<Api> api(new Api());
	if (!api->load_libraries(error) || !api->resolve_symbols(error))
		return nullptr;
	return api;
}

Api::~Api()
{
	if (cv_module_)
		FreeLibrary(cv_module_);
	if (ar_module_)
		FreeLibrary(ar_module_);
}

bool Api::load_libraries(std::string &error)
{
	sdk_dir_ = resolve_sdk_dir();
	const bool has_dir = !sdk_dir_.empty();
	const std::wstring ar_path = library_path(sdk_dir_, kArLibrary);
	const std::wstring cv_path = library_path(sdk_dir_, kCvLibrary);

	ar_module_ = load_library(ar_path, has_dir);
	if (!ar_module_) {
		error = "cannot load " + to_utf8(ar_path) + " (error " + std::to_string(GetLastError()) +
			"); is the NVIDIA AR SDK runtime installed?";
		return false;
	}

	cv_module_ = load_library(cv_path, has_dir);
	if (!cv_module_) {
		error = "cannot load " + to_utf8(cv_path) + " (error " + std::to_string(GetLastError()) + ")";
		return false;
	}

	version_ = read_version(ar_path);

	std::wstring model_dir = read_env(kModelDirEnv);
	if (model_dir.empty() && has_dir)
		model_dir = sdk_dir_ + L"\\models";
	model_dir_ = to_utf8(model_dir);
	return true;
}

bool Api::resolve_symbols(std::string &error)
{
	return resolve(ar_module_, "NvAR_Create", NvAR_Create, error) &&
	       resolve(ar_module_, "NvAR_Load", NvAR_Load, error) && resolve(ar_module_, "NvAR_Run", NvAR_Run, error) &&
	       resolve(ar_module_, "NvAR_Destroy", NvAR_Destroy, error) &&
	       resolve(ar_module_, "NvAR_CudaStreamCreate", NvAR_CudaStreamCreate, error) &&
	       resolve(ar_module_, "NvAR_CudaStreamDestroy", NvAR_CudaStreamDestroy, error) &&
	       resolve(ar_module_, "NvAR_SetString", NvAR_SetString, error) &&
	       resolve(ar_module_, "NvAR_SetCudaStream", NvAR_SetCudaStream, error) &&
	       resolve(ar_module_, "NvAR_SetObject", NvAR_SetObject, error) &&
	       resolve(ar_module_, "NvAR_SetF32Array", NvAR_SetF32Array, error) &&
	       resolve(cv_module_, "NvCV_GetErrorStringFromCode", NvCV_GetErrorStringFromCode, error) &&
	       resolve(cv_module_, "NvCVImage_Alloc", NvCVImage_Alloc, error) &&
	       resolve(cv_module_, "NvCVImage_Dealloc", NvCVImage_Dealloc, error) &&
	       resolve(cv_module_, "NvCVImage_InitFromD3D11Texture", NvCVImage_InitFromD3D11Texture, error) &&
	       resolve(cv_module_, "NvCVImage_MapResource", NvCVImage_MapResource, error) &&
	       resolve(cv_module_, "NvCVImage_UnmapResource", NvCVImage_UnmapResource, error) &&
	       resolve(cv_module_, "NvCVImage_Transfer", NvCVImage_Transfer, error);
}

const char *Api::error_string(NvCV_Status status) const
{
	const char *text = NvCV_GetErrorStringFromCode ? NvCV_GetErrorStringFromCode(status) : nullptr;
	return text ? text : "unknown NvCV error";
}

bool init_runtime()
{
	if (g_runtime)
		return true;

	g_runtime_error.clear();
	g_runtime = Api::load(g_runtime_error);
	if (!g_runtime) {
		obs_log(LOG_WARNING, "NVIDIA AR SDK unavailable: %s", g_runtime_error.c_str());
		return false;
	}

	obs_log(LOG_INFO, "NVIDIA AR SDK %s loaded, models: %s", g_runtime->version().c_str(),
		g_runtime->model_dir().empty() ? "<SDK default>" : g_runtime->model_dir().c_str());
	return true;
}

void shutdown_runtime()
{
	g_runtime.reset();
}

const Api *runtime()
{
	return g_runtime.get();
}

const std::string &runtime_error()
{
	return g_runtime_error;
}

} // namespace autoframe::nvar
