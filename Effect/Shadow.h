#pragma once

#include <DirectXMath.h>
#include "RenderToTexture.h"

namespace Effect
{
/*
 * �����Ƿ�װ��һ����Ⱦ��Ӱ���õ������/ģ�建����
 */
class Shadow final : public RenderToTexture {
public:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	Shadow(ID3D12Device* _device, UINT _width, DXGI_FORMAT _format);
	Shadow(const Shadow&) = delete;
	Shadow(Shadow&&) = default;
	Shadow& operator=(Shadow&&) = delete;
	~Shadow() override = default;
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void InitPSO(ID3D12Device* m_device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, UINT srvSize, UINT dsvSize, UINT dsvOffset);
	void RegisterMainLight(const Light* _mainLight);
	const XMMATRIX& GetShadowTransformXM() const;
protected:
	std::tuple<XMMATRIX, XMMATRIX, XMVECTOR, float, float> RegisterLightVPXM(const Light& light) const;
	void CreateDescriptors() override;
	void CreateResources() override;
private:
	const Light*	m_mainLight;
	XMMATRIX		m_shadowTransform;
};
}