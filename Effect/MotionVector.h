#pragma once

#include "RenderToTexture.h"

namespace Effect
{
class MotionVector final : public RenderToTexture
{
public:
	MotionVector(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	MotionVector(const MotionVector&) = delete;
	MotionVector& operator=(const MotionVector&) = delete;
	MotionVector(MotionVector&&) = default;
	MotionVector& operator=(MotionVector&&) = default;
	~MotionVector() override = default;
 
	void InitTexture(const string& _name);
	void InitShader(const wstring& motionVecName);
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT srvSize, UINT rtvSize);
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
private:
	void CreateDescriptors() override;
	void CreateResources() override;
private:
	std::unique_ptr<Shader>			m_shader;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuRTV;
	UINT							motionVectorSRVIdx;
	UINT							motionVectorRTVIdx;
};
}

