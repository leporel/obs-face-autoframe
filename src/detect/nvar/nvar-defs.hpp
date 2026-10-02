// Minimal subset of the NVIDIA AR SDK / NvCVImage C API used by this plugin.
//
// Only the runtime (DLLs + models) is installed on user machines, so the API is declared here and resolved at
// runtime with GetProcAddress. Layouts and values match the public SDK headers (nvAR.h, nvAR_defs.h, nvCVImage.h,
// nvCVStatus.h) of AR SDK 0.8.x.
#pragma once

#include <cstdint>

struct ID3D11Texture2D;
struct CUstream_st;

namespace autoframe::nvar {

using CUstream = CUstream_st *;
using FeatureHandle = void *;

enum NvCV_Status : int {
	NVCV_SUCCESS = 0,
	NVCV_ERR_GENERAL = -1,
	NVCV_ERR_SELECTOR = -5,
	NVCV_ERR_UNSUPPORTEDGPU = -17,
	NVCV_ERR_WRONGGPU = -18,
	NVCV_ERR_UNSUPPORTEDDRIVER = -19,
	NVCV_ERR_CUDA = -1099,
};

enum NvCVImage_PixelFormat : int {
	NVCV_FORMAT_UNKNOWN = 0,
	NVCV_RGBA = 6,
	NVCV_BGR = 5,
	NVCV_BGRA = 7,
};

enum NvCVImage_ComponentType : int {
	NVCV_TYPE_UNKNOWN = 0,
	NVCV_U8 = 1,
};

constexpr unsigned NVCV_CHUNKY = 0;
constexpr unsigned NVCV_PLANAR = 1;
constexpr unsigned NVCV_GPU = 1;

// Matches the data layout of the SDK's NvCVImage class (no virtual members).
struct NvCVImage {
	unsigned int width;
	unsigned int height;
	signed int pitch;
	NvCVImage_PixelFormat pixelFormat;
	NvCVImage_ComponentType componentType;
	unsigned char pixelBytes;
	unsigned char componentBytes;
	unsigned char numComponents;
	unsigned char planar;
	unsigned char gpuMem;
	unsigned char colorspace;
	unsigned char reserved[2];
	void *pixels;
	void *deletePtr;
	void (*deleteProc)(void *p);
	unsigned long long bufferBytes;
};

struct NvAR_Rect {
	float x;
	float y;
	float width;
	float height;
};

struct NvAR_BBoxes {
	NvAR_Rect *boxes;
	uint8_t num_boxes;
	uint8_t max_boxes;
};

// Parameter selector helpers, identical to the SDK macros.
#define NVAR_PARAM_INPUT(name) "NvAR_Parameter_Input_" #name
#define NVAR_PARAM_OUTPUT(name) "NvAR_Parameter_Output_" #name
#define NVAR_PARAM_CONFIG(name) "NvAR_Parameter_Config_" #name

constexpr const char *kFeatureFaceBoxDetection = "FaceBoxDetection";
constexpr const char *kFeatureBodyDetection = "BodyDetection";

// NvAR (nvARPose.dll)
using NvAR_Create_t = NvCV_Status (*)(const char *feature_id, FeatureHandle *handle);
using NvAR_Load_t = NvCV_Status (*)(FeatureHandle handle);
using NvAR_Run_t = NvCV_Status (*)(FeatureHandle handle);
using NvAR_Destroy_t = NvCV_Status (*)(FeatureHandle handle);
using NvAR_CudaStreamCreate_t = NvCV_Status (*)(CUstream *stream);
using NvAR_CudaStreamDestroy_t = NvCV_Status (*)(CUstream stream);
using NvAR_SetString_t = NvCV_Status (*)(FeatureHandle handle, const char *name, const char *value);
using NvAR_SetCudaStream_t = NvCV_Status (*)(FeatureHandle handle, const char *name, CUstream stream);
using NvAR_SetObject_t = NvCV_Status (*)(FeatureHandle handle, const char *name, void *ptr, unsigned long type_size);
using NvAR_SetF32Array_t = NvCV_Status (*)(FeatureHandle handle, const char *name, float *values, int count);

// NvCVImage (NVCVImage.dll)
using NvCV_GetErrorStringFromCode_t = const char *(*)(NvCV_Status code);
using NvCVImage_Alloc_t = NvCV_Status (*)(NvCVImage *image, unsigned width, unsigned height,
					  NvCVImage_PixelFormat format, NvCVImage_ComponentType type, unsigned layout,
					  unsigned mem_space, unsigned alignment);
using NvCVImage_Dealloc_t = void (*)(NvCVImage *image);
using NvCVImage_InitFromD3D11Texture_t = NvCV_Status (*)(NvCVImage *image, ID3D11Texture2D *texture);
using NvCVImage_MapResource_t = NvCV_Status (*)(NvCVImage *image, CUstream stream);
using NvCVImage_UnmapResource_t = NvCV_Status (*)(NvCVImage *image, CUstream stream);
using NvCVImage_Transfer_t = NvCV_Status (*)(const NvCVImage *src, NvCVImage *dst, float scale, CUstream stream,
					     NvCVImage *tmp);

} // namespace autoframe::nvar
