#pragma once

#include "RenderToTexture.h"
#include "Camera.h"

namespace Effect
{
class DynamicCubeMap final : public RenderToTexture {
public:
	DynamicCubeMap(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	~DynamicCubeMap() override = default;
	DynamicCubeMap(const DynamicCubeMap&) = delete;
	DynamicCubeMap& operator=(const DynamicCubeMap&) = delete;
	DynamicCubeMap(DynamicCubeMap&&) = default;
	DynamicCubeMap& operator=(DynamicCubeMap&&) = default;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT srvSize, UINT rtvSize, UINT dsvSize);
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void InitShader(const std::wstring& binaryName);
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void InitCamera(float x, float y, float z);
protected:
	void InitDSV(D3D12_CPU_DESCRIPTOR_HANDLE cpuDsvStart, UINT dsvSize);
	void InitDepthAndStencil();
	void CreateDescriptors() override;
	void CreateResources() override;
private:
	ComPtr<ID3D12Resource>			m_depthStencilRes;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuRtv[6];
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuDSV;
	UINT							m_rtvOffset;
	UINT							m_dsvOffset;
	FirstPersonCamera				m_cams[6];
	std::unique_ptr<Shader>			m_shader;
};
}
