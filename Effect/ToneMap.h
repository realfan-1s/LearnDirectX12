#pragma once
#include "RenderToTexture.h"

namespace Effect
{
class ToneMap final : public RenderToTexture {
public:
	ToneMap(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	~ToneMap() override;
	ToneMap(const ToneMap&) = delete;
	ToneMap& operator=(const ToneMap&) = delete;
	ToneMap(ToneMap&&) = default;
	ToneMap& operator=(ToneMap&&) = default;

	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvStart, UINT rtvOffset, UINT rtvSize, UINT srvSize);
	void InitShader(const wstring& binaryName);
	void InitTexture();

	template <UINT T>
	ID3D12Resource* GetBloomRes() const
	{
		static_assert(T < 2);
		return bloomRes[T].Get();
	}
	template <UINT T>
	const CD3DX12_CPU_DESCRIPTOR_HANDLE& GetRenderTarget() const
	{
		static_assert(T < 2);
		return bloomRTV[T];
	}
	template <UINT T1, D3D12_RESOURCE_STATES TBefore, D3D12_RESOURCE_STATES TAfter>
	void SetBloomState(ID3D12GraphicsCommandList* list) const
	{
		static_assert(T1 < 2);
		ChangeState<TBefore, TAfter>(list, bloomRes[T1].Get());
	}
	template <D3D12_RESOURCE_STATES TBefore, D3D12_RESOURCE_STATES TAfter>
	void SetResourceState(ID3D12GraphicsCommandList* list) const
	{
		ChangeState<TBefore, TAfter>(list, m_resource.Get());
	}
private:
	void CreateResources() override;
	void CreateDescriptors() override;
private:
	std::unique_ptr<Shader>								m_shader;
	ComPtr<ID3D12Resource>								bloomRes[2];
	CD3DX12_CPU_DESCRIPTOR_HANDLE						bloomRTV[2];
	CD3DX12_CPU_DESCRIPTOR_HANDLE						bloomCpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE						bloomGpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE						toneMapUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE						toneMapUAV_GPU;
	UINT												bloomIdx;
	UINT												toneMapIdx;
};
}

