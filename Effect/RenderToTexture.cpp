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
	return m_resources.Get();
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
		CreateDescriptors();
		CreateResources();
	}
}

void Effect::RenderToTexture::InitDepthAndStencil(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_DESC dsDesc;
	dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.Alignment = 0;
	dsDesc.Width = m_width;
	dsDesc.Height = m_height;
	dsDesc.DepthOrArraySize = 1;
	dsDesc.MipLevels = 1;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &dsDesc, D3D12_RESOURCE_STATE_COMMON, &optClear, IID_PPV_ARGS(m_depthStencilRes.GetAddressOf())));
	}

	m_device->CreateDepthStencilView(m_depthStencilRes.Get(), nullptr, m_cpuDSV);
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilRes.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		cmdList->ResourceBarrier(1, &trans);
	}
}

void Effect::RenderToTexture::InitShader(const std::wstring& binaryName) {
	m_shader = make_unique<Shader>(default_shader, binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC>({ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} }));
}

void Effect::RenderToTexture::InitSRV(std::string_view name) {
	m_srvIndex = TextureMgr::instance().RegisterRenderToTexture(name);
}

void Effect::RenderToTexture::InitDSV(CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandler)
{
	m_cpuDSV = dsvHandler;
}

ID3D12PipelineState* Effect::RenderToTexture::GetPSO() const
{
	return m_pso.Get();
}

UINT Effect::RenderToTexture::GetSrvIdx() const
{
	return m_srvIndex;
}