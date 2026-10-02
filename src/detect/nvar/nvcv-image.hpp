// RAII wrapper for NvCVImage: either a GPU buffer owned by the SDK or a view of a registered D3D11 texture.
#pragma once

#include "detect/nvar/nvar-api.hpp"

namespace autoframe::nvar {

class Image {
public:
	explicit Image(const Api &api) : api_(api) {}
	~Image() { reset(); }
	Image(const Image &) = delete;
	Image &operator=(const Image &) = delete;

	// Allocates an 8-bit GPU image. Re-allocation releases the previous buffer.
	NvCV_Status alloc_gpu(unsigned width, unsigned height, NvCVImage_PixelFormat format, unsigned layout);

	// Registers a D3D11 texture for CUDA interop. The texture must outlive this image or be re-wrapped first.
	NvCV_Status wrap_d3d11(ID3D11Texture2D *texture);

	void reset();

	[[nodiscard]] bool empty() const { return !allocated_; }
	[[nodiscard]] unsigned width() const { return image_.width; }
	[[nodiscard]] unsigned height() const { return image_.height; }
	[[nodiscard]] NvCVImage *get() { return &image_; }
	[[nodiscard]] ID3D11Texture2D *wrapped_texture() const { return wrapped_; }

private:
	const Api &api_;
	NvCVImage image_{};
	ID3D11Texture2D *wrapped_ = nullptr;
	bool allocated_ = false;
};

// Maps a D3D11-backed image for CUDA access for the lifetime of the scope.
class MappedImage {
public:
	MappedImage(const Api &api, Image &image, CUstream stream);
	~MappedImage();
	MappedImage(const MappedImage &) = delete;
	MappedImage &operator=(const MappedImage &) = delete;

	[[nodiscard]] NvCV_Status status() const { return status_; }

private:
	const Api &api_;
	Image &image_;
	CUstream stream_;
	NvCV_Status status_;
};

} // namespace autoframe::nvar
