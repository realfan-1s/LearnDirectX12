#include <DirectXColors.h>
#include "DeferShading.h"
#include "Texture.h"
#include "Mesh.h"
#include "d3dcompiler.h"
#include "RtvDsvMgr.h"

using namespace DirectX;

Renderer::DeferShading::DeferShading(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: IRenderer(_device, _width, _height, _format){
	m_rtvOffset = RtvDsvMgr::instance().RegisterRTV(2);
	CreateResources();
}

void Renderer::DeferShading::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc = templateDesc;
	graphicsDesc.InputLayout = { m_shaderPack[L"Shaders\\Box"]->GetInputLayouts(), m_shaderPack[L"Shaders\\Box"]->GetInputLayoutSize() };
	graphicsDesc.VS = { static_cast<BYTE*>(m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	graphicsDesc.PS = { static_cast<BYTE*>(m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()), m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	graphicsDesc.DepthStencilState.DepthEnable = false;
	graphicsDesc.NumRenderTargets = 2u;
	graphicsDesc.RTVFormats[0] = m_format;
	graphicsDesc.RTVFormats[1] = m_format;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&graphicsDesc, IID_PPV_ARGS(&m_pso)));
}

void Renderer::DeferShading::InitTexture()
{
	m_bloomIdx = TextureMgr::instance().RegisterRenderToTexture("Bloom0");
	TextureMgr::instance().RegisterRenderToTexture("Bloom1");
}

void Renderer::DeferShading::InitDSV(D3D12_CPU_DESCRIPTOR_HANDLE _cpuDSV) {
	m_cpuDSV = _cpuDSV;
}

void Renderer::DeferShading::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart,
	D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart,
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, UINT srvSize, UINT rtvSize, UINT dsvSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandler(srvCpuStart, static_cast<INT>(m_bloomIdx), srvSize);
	m_cpuSRV = srvCpuHandler;
 	m_bloomCpuSRV = srvCpuHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandler(srvGpuStart, static_cast<INT>(m_bloomIdx), srvSize);
	m_gpuSRV = srvGpuHandler;
	m_bloomGpuSRV = srvGpuHandler.Offset(1, srvSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuHandler(rtvCpuStart, static_cast<INT>(m_rtvOffset), rtvSize);
	m_bloomRTV[0] = rtvCpuHandler;
	m_bloomRTV[1] = rtvCpuHandler.Offset(1, rtvSize);

	CreateDescriptors();
}
  
void Renderer::DeferShading::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
	cmdList->ClearRenderTargetView(m_bloomRTV[0], Colors::LightSteelBlue, 0, nullptr);
	cmdList->ClearRenderTargetView(m_bloomRTV[1], Colors::LightSteelBlue, 0, nullptr);
	cmdList->OMSetRenderTargets(2, &m_bloomRTV[0], true, &m_cpuDSV);

	cmdList->SetPipelineState(m_pso.Get());
	cmdList->SetName(L"Direct Light Draw");
	DrawCanvas(cmdList);
}

void Renderer::DeferShading::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) {
}

void Renderer::DeferShading::OnResize(UINT newWidth, UINT newHeight) {
	if (m_width != newWidth || m_height != newHeight)
	{
			m_width = newWidth;
			m_height = newHeight;
			CreateResources();
			CreateDescriptors();
			m_dirtyFlag = true;
	}
}

void Renderer::DeferShading::CreateResources() {
	D3D12_RESOURCE_DESC rtvDesc{};
	ZeroMemory(&rtvDesc, sizeof(D3D12_RESOURCE_DESC));
	rtvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rtvDesc.Alignment = 0;
	rtvDesc.Format = m_format;
	rtvDesc.Width = m_width;
	rtvDesc.Height = m_height;
	rtvDesc.DepthOrArraySize = 1;
	rtvDesc.SampleDesc.Count = 1;
	rtvDesc.SampleDesc.Quality = 0;
	rtvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	rtvDesc.MipLevels = 1;
	rtvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &rtvDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_resource)));
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &rtvDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_bloomRes)));
	}
}

void Renderer::DeferShading::CreateDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = m_format;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	m_device->CreateShaderResourceView(m_bloomRes.Get(), &srvDesc, m_bloomCpuSRV);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = m_format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_bloomRTV[0]);
	m_device->CreateRenderTargetView(m_bloomRes.Get(), &rtvDesc, m_bloomRTV[1]);
}
