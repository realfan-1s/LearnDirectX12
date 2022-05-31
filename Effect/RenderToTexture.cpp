#include "RenderToTexture.h"
#include "D3DUtil.hpp"
#include "Texture.h"

using namespace Effect;

RenderToTexture::RenderToTexture(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: m_device(_device), m_width(_width), m_height(_height), m_format(_format)
{
}

const CD3DX12_CPU_DESCRIPTOR_HANDLE& RenderToTexture::GetCpuSRV() const {
	return m_cpuSRV;
}

const CD3DX12_GPU_DESCRIPTOR_HANDLE& RenderToTexture::GetGpuSRV() const {
	return m_gpuSRV;
}

ID3D12Resource* RenderToTexture::GetResource() const {
	return m_resource.Get();
}

const D3D12_VIEWPORT& RenderToTexture::GetViewPort() const {
	return m_viewport;
}

const D3D12_RECT& RenderToTexture::GetScissorRect() const {
	return m_scissorRect;
}

void RenderToTexture::OnResize(UINT newWidth, UINT newHeight) {
	if (m_width != newWidth || m_height != newHeight)
	{
		m_width = newWidth;
		m_height = newHeight;
		CreateResources();
		CreateDescriptors();
		m_dirtyFlag = true;
	}
}

ID3D12PipelineState* RenderToTexture::GetPSO() const
{
	return m_pso.Get();
}

std::optional<UINT> RenderToTexture::GetSrvIdx(std::string_view name)
{
	return TextureMgr::instance().GetRegisterType(name);
}

void RenderToTexture::SetViewPortAndScissor()
{
	m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
	m_scissorRect = { 0, 0, static_cast<int>(m_width), static_cast<int>(m_height) };
}
