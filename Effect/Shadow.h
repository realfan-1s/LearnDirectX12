#pragma once

#include <DirectXMath.h>
#include "RenderToTexture.h"
#include "Scene.h"

namespace Effect
{
/*
 * 本质是封装了一个渲染阴影会用到的深度/模板缓冲区
 */
class Shadow final : public RenderToTexture {
public:
	Shadow(ID3D12Device* _device, UINT _width);
	Shadow(const Shadow&) = delete;
	Shadow(Shadow&&) = default;
	Shadow& operator=(Shadow&&) = delete;
	~Shadow() override = default;
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void InitShader(const std::wstring& binaryName);
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, UINT srvSize, UINT dsvSize);
	void RegisterMainLight(const Light* _mainLight);
	const XMMATRIX& GetShadowTransformXM() const;
protected:
	std::tuple<XMMATRIX, XMMATRIX, XMVECTOR, float, float> RegisterLightVPXM() const;
	void CreateDescriptors() override;
	void CreateResources() override;
private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	using Scene = Models::Scene;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuDSV;
	const Light*					m_mainLight;
	XMMATRIX						m_shadowTransform;
	std::unique_ptr<Shader>			m_shader;
	UINT							m_dsvOffset;
};
}
