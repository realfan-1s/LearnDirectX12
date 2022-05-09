#include "GBuffer.h"
#include "D3DUtil.hpp"
#include <DirectXColors.h>

using namespace Renderer;
using namespace DirectX;

GBuffer::GBuffer(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _albedoFormat, DXGI_FORMAT _posFormat, DXGI_FORMAT _normalFormat)
: GBufferFormat(_albedoFormat, _posFormat, _normalFormat), m_device(_device), m_width(_width), m_height(_height) {
	CreateResources();
}

void GBuffer::Resize(UINT newWidth, UINT newHeight) {
	if (m_width != newWidth && m_height != newHeight)
	{
		m_width = newWidth;
		m_height = newHeight;
		CreateResources();
		CreateDescriptors();
	}
}

void GBuffer::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvStart, UINT rtvOffset, UINT srvSize, UINT rtvSize) {
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandler(srvCpuStart, (INT)albedoIdx, srvSize);
	gBufferCpuSRV[0] = srvHandler;
	gBufferCpuSRV[1] = srvHandler.Offset(1, srvSize);
	gBufferCpuSRV[2] = srvHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrvHandler(srvGpuStart, (INT)albedoIdx, srvSize);
	gBufferGpuSRV[0] = gpuSrvHandler;
	gBufferGpuSRV[1] = gpuSrvHandler.Offset(1, srvSize);
	gBufferGpuSRV[2] = gpuSrvHandler.Offset(1, srvSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandler(rtvStart, (INT)rtvOffset, rtvSize);
	gBufferRTV[0] = rtvHandler;
	gBufferRTV[1] = rtvHandler.Offset(1, rtvSize);
	gBufferRTV[2] = rtvHandler.Offset(1, rtvSize);

	CreateDescriptors();
}

void Renderer::GBuffer::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	auto opaqueDesc = templateDesc;
	opaqueDesc.InputLayout = { m_shader[L"Shaders\\GBuffer"]->GetInputLayouts(), m_shader[L"Shaders\\GBuffer"]->GetInputLayoutSize() };
	opaqueDesc.VS = { m_shader[L"Shaders\\GBuffer"]->GetShaderByType(ShaderPos::vertex)->GetBufferPointer(), m_shader[L"Shaders\\GBuffer"]->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	opaqueDesc.PS = { m_shader[L"Shaders\\GBuffer"]->GetShaderByType(ShaderPos::fragment)->GetBufferPointer(), m_shader[L"Shaders\\GBuffer"]->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	opaqueDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	opaqueDesc.NumRenderTargets = 3;
	opaqueDesc.RTVFormats[0] = albedoFormat;
	opaqueDesc.RTVFormats[1] = posFormat;
	opaqueDesc.RTVFormats[2] = normalFormat;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaqueDesc, IID_PPV_ARGS(&m_pso)));
}

void Renderer::GBuffer::RefreshGBuffer(ID3D12GraphicsCommandList* cmdList) {
	for (int i = 0; i < 3; ++i)
	{
		cmdList->ClearRenderTargetView(gBufferRTV[i], Colors::LightSteelBlue, 0, nullptr);
	}
}

void GBuffer::CreateResources() {
	D3D12_RESOURCE_DESC gBufferDesc;
	ZeroMemory(&gBufferDesc, sizeof(D3D12_RESOURCE_DESC));
	gBufferDesc.Width = m_width;
	gBufferDesc.Height = m_height;
	gBufferDesc.MipLevels = 1;
	gBufferDesc.Alignment = 0;
	gBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	gBufferDesc.DepthOrArraySize = 1;
	gBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	gBufferDesc.SampleDesc.Count = 1;
	gBufferDesc.SampleDesc.Quality = 0;
	gBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		gBufferDesc.Format = albedoFormat;
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &gBufferDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&gBufferRes[0])));
		gBufferDesc.Format = posFormat;
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &gBufferDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&gBufferRes[1])));
		gBufferDesc.Format = normalFormat;
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &gBufferDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&gBufferRes[2])));
	}
}

void GBuffer::CreateDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = albedoFormat;
	m_device->CreateShaderResourceView(gBufferRes[0].Get(), &srvDesc, gBufferCpuSRV[0]);
	srvDesc.Format = posFormat;
	m_device->CreateShaderResourceView(gBufferRes[1].Get(), &srvDesc, gBufferCpuSRV[1]);
	srvDesc.Format = normalFormat;
	m_device->CreateShaderResourceView(gBufferRes[2].Get(), &srvDesc, gBufferCpuSRV[2]);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	rtvDesc.Format = albedoFormat;
	m_device->CreateRenderTargetView(gBufferRes[0].Get(), &rtvDesc, gBufferRTV[0]);
	rtvDesc.Format = posFormat;
	m_device->CreateRenderTargetView(gBufferRes[1].Get(), &rtvDesc, gBufferRTV[1]);
	rtvDesc.Format = normalFormat;
	m_device->CreateRenderTargetView(gBufferRes[2].Get(), &rtvDesc, gBufferRTV[2]);
}
