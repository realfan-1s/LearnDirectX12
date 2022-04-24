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
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT rtvOffset, UINT srvSize, UINT rtvSize);
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void InitPSO(ID3D12Device* m_device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void InitCamera(float x, float y, float z);
protected:
	void CreateDescriptors() override;
	void CreateResources() override;
private:
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuRtv[6];
	FirstPersonCamera				m_cams[6];
};
}
