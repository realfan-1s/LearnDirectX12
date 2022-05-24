#pragma once
#include "RenderToTexture.h"
#include "MotionVector.h"
#include "Mesh.h"

namespace Effect
{
class TemporalAA final : public RenderToTexture
{
public:
	TemporalAA(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	TemporalAA(const TemporalAA&) = delete;
	TemporalAA& operator=(const TemporalAA&) = delete;
	TemporalAA(TemporalAA&&) = default;
	TemporalAA& operator=(TemporalAA&&) = default;
	~TemporalAA() override = default;

	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void InitShader(const wstring& taaName, const wstring& motionVecName);
	void InitTexture(const string& temporalName, const string& prevName, const string& motionVecName);
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void OnResize(UINT newWidth, UINT newHeight) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void FirstDraw(ID3D12GraphicsCommandList* cmdList, const D3D12_CPU_DESCRIPTOR_HANDLE& depthHandler,
		ID3D12RootSignature* signature, const std::function<void()>& drawFunc);
	void FirstDraw(ID3D12GraphicsCommandList* cmdList, const D3D12_CPU_DESCRIPTOR_HANDLE& depthHandler, const std::function<void()>& drawFunc) const;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT srvSize, UINT rtvSize);
	XMFLOAT2 GetJitter() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetMotionVector() const;
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetPrevRTV() const;
private:
	XMFLOAT2 GetPrevJitter() const;
	void CreateResources() override;
	void CreateDescriptors() override;
private:
	CD3DX12_CPU_DESCRIPTOR_HANDLE				m_cpuUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE				m_gpuUAV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE				m_prevCpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE				m_prevGpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE				m_prevCpuUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE				m_prevGpuUAV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE				m_prevCpuRTV;
	ComPtr<ID3D12Resource>						m_prevResource;
	ComPtr<ID3D12PipelineState>					m_firstPso;
	std::unique_ptr<Shader>						m_shader;
	std::unique_ptr<Shader>						m_firstShader;
	std::unique_ptr<MotionVector>				m_motionVector;
	UINT										TAASrvIdx;
	UINT										TAAUavIdx;
	UINT										prevSrvIdx;
	UINT										prevUavIdx;
	UINT										prevRtvIdx;
	mutable UINT								jitterPivot;
};
}

