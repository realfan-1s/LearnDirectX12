#pragma once
#include "IRenderer.h"
#include "UploaderBuffer.hpp"

namespace Renderer
{
class TileBasedDefer final : public IRenderer
{
public:
	TileBasedDefer(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	TileBasedDefer(const TileBasedDefer&) = delete;
	TileBasedDefer& operator=(const TileBasedDefer&) = delete;
	TileBasedDefer(TileBasedDefer&&) = default;
	TileBasedDefer& operator=(TileBasedDefer&&) = default;
	~TileBasedDefer() override = default;

	void InitRootSignature();
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void InitTexture() override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, UINT srvSize, UINT rtvSize, UINT dsvSize) override;

	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void UpdatePointLights(const vector<std::shared_ptr<Light<Compute>>>& points);
private:
	auto GetStaticSampler()->std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> const;
	void InitDSV(D3D12_CPU_DESCRIPTOR_HANDLE _cpuDSV, UINT dsvSize) override;
	void CreateDescriptors() override;
	void CreateResources() override;
private:
	ComPtr<ID3D12RootSignature>						m_pointRootSig;
	ComPtr<ID3D12PipelineState>						m_pointPso;
	std::unique_ptr<UploaderBuffer<LightInCompute>>	m_pointLightUploader;
	CD3DX12_CPU_DESCRIPTOR_HANDLE					m_bloomCpuUAV[2];
	CD3DX12_GPU_DESCRIPTOR_HANDLE					m_bloomGpuUAV[2];
};
}

