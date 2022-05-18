#pragma once

#include "RenderToTexture.h"
#include "UploaderBuffer.hpp"
#include "BilateralBlur.hpp"

namespace Effect
{
class SSAO final : public RenderToTexture {
public:
	SSAO(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	SSAO(const SSAO&) = delete;
	SSAO& operator=(const SSAO&) = delete;
	SSAO(SSAO&&) = default;
	SSAO& operator=(SSAO&&) = default;
	~SSAO() override = default;

	void OnResize(UINT newWidth, UINT newHeight) override;
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void InitTexture();
	void InitShader();
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void CreateRandomTexture(ID3D12GraphicsCommandList* cmdList);
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuSRVStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSRVStart, D3D12_CPU_DESCRIPTOR_HANDLE cpuRTVStart, UINT srvSize, UINT rtvSize);
private:
	void CreateDescriptors() override;
	void CreateResources() override;
private:
	struct SSAOPass {
		XMFLOAT4 offset[14];
		float    radius;
		float    surfaceEpsilon;
		float    attenuationEnd;
		float    attenuationStart;
	};
	D3D12_CPU_DESCRIPTOR_HANDLE						m_cpuRTV;
	D3D12_CPU_DESCRIPTOR_HANDLE						randomCpuSRV;
	D3D12_GPU_DESCRIPTOR_HANDLE						randomGpuSRV;
	ComPtr<ID3D12Resource>							randomTex;
	ComPtr<ID3D12Resource>							randomUploader;
	std::unique_ptr<Shader>							ssaoShader;
	UINT											rtvIdx;
	UINT											resIdx;
	UINT											randomIdx;
	UINT											m_srvSize;
	std::unique_ptr<UploaderBuffer<SSAOPass>>		m_ssaoUploader;
	std::unique_ptr<BilateralBlur<blurByCompute>>	m_bilateralBlur;
};
}

