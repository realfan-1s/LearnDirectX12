#pragma once

#include "Renderer/IRenderer.h"
#include "Light.h"

namespace Renderer
{
// 只负责最终延迟着色的部分
class DeferShading final : public IRenderer {
public:
	DeferShading(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	~DeferShading() override = default;
	DeferShading(const DeferShading&) = delete;
	DeferShading& operator=(const DeferShading&) = delete;
	DeferShading(DeferShading&&) = default;
	DeferShading& operator=(DeferShading&&) = default;

	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void InitTexture() override;
	void InitDSV(D3D12_CPU_DESCRIPTOR_HANDLE _cpuDSV) override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, UINT rtvOffset, UINT dsvOffset, UINT srvSize, UINT rtvSize, UINT dsvSize) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void OnResize(UINT newWidth, UINT newHeight) override;
private:
	void CreateResources() override;
	void CreateDescriptors() override;
};
}

