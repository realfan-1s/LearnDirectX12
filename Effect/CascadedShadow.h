#pragma once

#include "Camera.h"
#include "RenderToTexture.h"
#include "UploaderBuffer.hpp"

namespace Effect
{
/*
 * 将视锥体划分成多个子视锥体
 * 为每个子视锥体计算光照空间下的正交投影
 * 为每个子视锥体渲染shadowMap
 */
class CascadedShadow final : public RenderToTexture
{
public:
	CascadedShadow(ID3D12Device* _device, UINT _width);
	CascadedShadow(const CascadedShadow&) = delete;
	CascadedShadow& operator=(const CascadedShadow&) = delete;
	CascadedShadow(CascadedShadow&&) = default;
	CascadedShadow& operator=(CascadedShadow&&) = default;
	~CascadedShadow() override = default;

	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvStart, D3D12_CPU_DESCRIPTOR_HANDLE cpuDsvStart, UINT srvSize, UINT dsvSize);
	void InitShader(const wstring& csmName);
	void InitTexture(string_view csmName);
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void SetNecessaryParameters(float _offset, float _range, const shared_ptr<Camera>& _viewCam, const Light<Pixel>* _mainLight, int kernelSize);
	void CopyCascadedShadowPass(ID3D12GraphicsCommandList* cmdList) const;
	XMFLOAT4X4 GetShadowView() const;
	const XMMATRIX& GetShadowViewXM() const;
	ID3D12Resource* GetCascadedRes(UINT idx) const;
	UINT GetCascadedSrvOffset() const;
private:
	void SyncWithShadowPass();
	std::tuple<XMMATRIX, XMVECTOR> RegisterLightViewXM() const;
	void CreateResources() override;
	void CreateDescriptors() override;
private:
	struct CascadedShadowPass {
		XMFLOAT4	cascadedShadowPercent_gpu[2]; // 视锥体所占的百分比
		XMFLOAT4	cascadedOffset_gpu[5];
		XMFLOAT4	cascadedScale_gpu[5];
		XMFLOAT4	cascadedDepthFloat_gpu[5];
		float		cascadedBlend_gpu{ 0.2f };
		float		cascadedShadowOffset;
		int			pcfStart_gpu;
		int			pcfEnd_gpu;
	};

	std::unique_ptr<UploaderBuffer<CascadedShadowPass>> cascadedUploader;
	ComPtr<ID3D12Resource>								m_shadowRes[4];
	CD3DX12_CPU_DESCRIPTOR_HANDLE						m_cpuDSV[5];
	CD3DX12_CPU_DESCRIPTOR_HANDLE						m_shadowCpuSRV[4];
	CD3DX12_GPU_DESCRIPTOR_HANDLE						m_shadowGpuSRV[4];
	XMMATRIX											m_shadowView;
	XMMATRIX											m_shadowProj[5]{};
	const Light<Pixel>*									m_mainLight;
	UINT												m_dsvOffset;
	UINT												m_srvOffset;
	INT													m_kernelSize;
	UINT												m_passOffset;
	float												m_shadowOffset;
	float												m_cascadedBlend{ 0.2f };
	float												m_depthFloatFrustum[5];
	std::unique_ptr<Shader>								m_shader;
	std::shared_ptr<Camera>								m_camera;
public:
	static constexpr UINT								cascadeLevels = 5U;
	static constexpr float								cascadedPercent[cascadeLevels] = { 0.05f, 0.10f, 0.22f, 0.3f, 0.4f };
};
}

