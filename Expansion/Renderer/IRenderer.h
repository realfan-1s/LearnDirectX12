#pragma once

#include <utility>
#include <array>

#include "BaseGeometry.h"
#include "Mesh.h"
#include "RenderToTexture.h"
#include "Texture.h"

namespace Renderer
{
struct IRenderer :public Effect::RenderToTexture {
public:
	IRenderer(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
	: RenderToTexture(_device, _width, _height, _format) {}
	IRenderer(const IRenderer&) = delete;
	IRenderer& operator=(const IRenderer&) = delete;
	IRenderer& operator=(IRenderer&&) = default;
	IRenderer(IRenderer&&) = default;
	virtual ~IRenderer() override = default;
	virtual void InitTexture() = 0;
	virtual void InitDSV(D3D12_CPU_DESCRIPTOR_HANDLE _cpuDSV) = 0;
	virtual void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, UINT rtvOffset, UINT dsvOffset, UINT srvSize, UINT rtvSize, UINT dsvSize) = 0;
	template <typename... Args>
	void InitShaders(Args&&... args) noexcept
	{
		((m_shaderPack[std::forward<decltype(std::get<0>(args))>(std::get<0>(args))] = 
			std::make_unique<Shader>(
				std::forward<decltype(std::get<1>(args))>(std::get<1>(args)), 
				std::forward<decltype(std::get<0>(args))>(std::get<0>(args)), 
				std::forward<decltype(std::get<2>(args))>(std::get<2>(args)))), ...);
	}
	ID3D12Resource* GetBloomRes() const
	{
		return m_bloomRes.Get();
	}
	template <UINT T>
	const CD3DX12_CPU_DESCRIPTOR_HANDLE& GetRenderTarget() const
	{
		static_assert(T < 2);
		return m_bloomRTV[T];
	}
	template <UINT T>
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetBloomSRV() const
	{
		static_assert(T < 2);
		if constexpr (T == 0)
			return m_gpuSRV;
		else
			return m_bloomGpuSRV;
	}
	template <UINT T1, D3D12_RESOURCE_STATES TBefore, D3D12_RESOURCE_STATES TAfter>
	void SetBloomState(ID3D12GraphicsCommandList* list) const
	{
		if constexpr (T1 == 0)
			ChangeState<TBefore, TAfter>(list, m_resource.Get());
		else
			ChangeState<TBefore, TAfter>(list, m_bloomRes.Get());
	}
protected:
	unordered_map<std::wstring, std::unique_ptr<Shader>>	m_shaderPack;
	ComPtr<ID3D12Resource>									m_bloomRes;
	CD3DX12_CPU_DESCRIPTOR_HANDLE							m_cpuDSV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE							m_bloomRTV[2];
	CD3DX12_CPU_DESCRIPTOR_HANDLE							m_bloomCpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE							m_bloomGpuSRV;
	UINT													m_bloomIdx;
};
}