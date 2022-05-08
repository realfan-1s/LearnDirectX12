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
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, UINT srvSize);
	void InitShader(const wstring& binaryName);
	void InitTexture();

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
	CD3DX12_CPU_DESCRIPTOR_HANDLE						toneMapUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE						toneMapUAV_GPU;
	UINT												toneMapIdx;
};
}

