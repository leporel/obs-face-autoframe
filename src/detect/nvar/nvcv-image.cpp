#include "detect/nvar/nvcv-image.hpp"

#include "util/log.hpp"

namespace autoframe::nvar {
namespace {

// Row alignment in bytes for SDK-allocated buffers; 1 means tightly packed, as used by NVIDIA samples.
constexpr unsigned kTightAlignment = 1;

} // namespace

NvCV_Status Image::alloc_gpu(unsigned width, unsigned height, NvCVImage_PixelFormat format, unsigned layout)
{
	reset();
	const NvCV_Status status =
		api_.NvCVImage_Alloc(&image_, width, height, format, NVCV_U8, layout, NVCV_GPU, kTightAlignment);
	allocated_ = status == NVCV_SUCCESS;
	return status;
}

NvCV_Status Image::wrap_d3d11(ID3D11Texture2D *texture)
{
	reset();
	const NvCV_Status status = api_.NvCVImage_InitFromD3D11Texture(&image_, texture);
	allocated_ = status == NVCV_SUCCESS;
	wrapped_ = allocated_ ? texture : nullptr;
	return status;
}

void Image::reset()
{
	if (allocated_)
		api_.NvCVImage_Dealloc(&image_);

	image_ = NvCVImage{};
	wrapped_ = nullptr;
	allocated_ = false;
}

MappedImage::MappedImage(const Api &api, Image &image, CUstream stream)
	: api_(api),
	  image_(image),
	  stream_(stream),
	  status_(api.NvCVImage_MapResource(image.get(), stream))
{
}

MappedImage::~MappedImage()
{
	if (status_ != NVCV_SUCCESS)
		return;

	if (const NvCV_Status status = api_.NvCVImage_UnmapResource(image_.get(), stream_); status != NVCV_SUCCESS)
		obs_log(LOG_WARNING, "NvCVImage_UnmapResource failed: %s", api_.error_string(status));
}

} // namespace autoframe::nvar
