#include "detect/nvar/gpu-check.hpp"

#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace autoframe::nvar {
namespace {

constexpr UINT kNvidiaVendorId = 0x10DE;

std::string adapter_description(const DXGI_ADAPTER_DESC &desc)
{
	char *utf8 = nullptr;
	if (os_wcs_to_utf8_ptr(desc.Description, 0, &utf8) == 0 || !utf8)
		return {};

	std::string name(utf8);
	bfree(utf8);
	return name;
}

} // namespace

RendererGpu query_renderer_gpu()
{
	RendererGpu result;
	if (gs_get_device_type() != GS_DEVICE_DIRECT3D_11)
		return result;
	result.is_d3d11 = true;

	auto *device = static_cast<ID3D11Device *>(gs_get_device_obj());
	if (!device)
		return result;

	Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
		return result;

	Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
	if (FAILED(dxgi_device->GetAdapter(&adapter)))
		return result;

	DXGI_ADAPTER_DESC desc{};
	if (FAILED(adapter->GetDesc(&desc)))
		return result;

	result.is_nvidia = desc.VendorId == kNvidiaVendorId;
	result.adapter_name = adapter_description(desc);
	return result;
}

} // namespace autoframe::nvar
