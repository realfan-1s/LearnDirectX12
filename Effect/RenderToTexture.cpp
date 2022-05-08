#include "RenderToTexture.h"
#include "D3DUtil.hpp"
#include "Texture.h"

Effect::RenderToTexture::RenderToTexture(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: m_device(_device), m_width(_width), m_height(_height), m_format(_format)
{
	m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
	m_scissorRect = { 0, 0, static_cast<int>(m_width), static_cast<int>(m_height) };
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Effect::RenderToTexture::GetCpuSRV() const {
	return m_cpuSRV;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Effect::RenderToTexture::GetGpuSRV() const {
	return m_gpuSRV;
}

ID3D12Resource* Effect::RenderToTexture::GetResource() const {
	return m_resource.Get();
}

D3D12_VIEWPORT Effect::RenderToTexture::GetViewPort() const {
	return m_viewport;
}

D3D12_RECT Effect::RenderToTexture::GetScissorRect() const {
	return m_scissorRect;
}

void Effect::RenderToTexture::OnResize(UINT newWidth, UINT newHeight) {
	if (m_width != newWidth || m_height != newHeight)
	{
		m_width = newWidth;
		m_height = newHeight;
		CreateResources();
		CreateDescriptors();
		m_dirtyFlag = true;
	}
}

void Effect::RenderToTexture::InitSRV(std::string_view name) {
	TextureMgr::instance().RegisterRenderToTexture(name);
}

ID3D12PipelineState* Effect::RenderToTexture::GetPSO() const
{
	return m_pso.Get();
}

std::optional<UINT> Effect::RenderToTexture::GetSrvIdx(std::string_view name)
{
	return TextureMgr::instance().GetRegisterType(name);
}
