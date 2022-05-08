#pragma once
#include <Windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <Src/d3dx12.h>

#include "Mesh.h"
#include "Shader.h"

namespace Renderer
{
struct GBufferFormat {
	constexpr GBufferFormat(DXGI_FORMAT _albedoFormat, DXGI_FORMAT _posFormat, DXGI_FORMAT _normalFormat)
	: albedoFormat(_albedoFormat), posFormat(_posFormat), normalFormat(_normalFormat) {}
	DXGI_FORMAT	albedoFormat;
	DXGI_FORMAT	posFormat;
	DXGI_FORMAT	normalFormat;
};
struct GBuffer : public GBufferFormat {
private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
public:
	GBuffer(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _albedoFormat, DXGI_FORMAT _posFormat, DXGI_FORMAT _normalFormat);
	void Resize(UINT newWidth, UINT newHeight);
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvStart, UINT rtvOffset, UINT srvSize, UINT rtvSize);
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc);
	void RefreshGBuffer(ID3D12GraphicsCommandList* cmdList);
	template <typename... Args>
	void InitShaders(Args&&... args) noexcept;
private:
	void CreateResources();
	void CreateDescriptors();
public:
	ComPtr<ID3D12PipelineState>						m_pso;
	ComPtr<ID3D12Device>							m_device;
	ComPtr<ID3D12Resource>							gBufferRes[3];
	CD3DX12_CPU_DESCRIPTOR_HANDLE					gBufferRTV[3];
	CD3DX12_CPU_DESCRIPTOR_HANDLE					gBufferCpuSRV[3]; // albedo¡¢pos¡¢normal¡¢depth(ÔÝÊ±ÏÈ²»¿¼ÂÇ)
	CD3DX12_GPU_DESCRIPTOR_HANDLE					gBufferGpuSRV[3];
	UINT											albedoIdx;
	UINT											depthIdx;
	UINT											normalIdx;
	UINT											m_width;
	UINT											m_height;
	unordered_map<std::wstring, unique_ptr<Shader>>	m_shader;
};

template<typename... Args>
inline void GBuffer::InitShaders(Args&&... args) noexcept {
	((m_shader[std::forward<decltype(std::get<0>(args))>(std::get<0>(args))] = std::make_unique<Shader>(
	std::forward<decltype(std::get<1>(args))>(std::get<1>(args)), 
	std::forward<decltype(std::get<0>(args))>(std::get<0>(args)), 
	std::forward<decltype(std::get<2>(args))>(std::get<2>(args)))), ...);
}
}


