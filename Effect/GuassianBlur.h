#pragma once
#include "RenderToTexture.h"
#include <array>
#include "TexSizeChange.h"

namespace Effect
{
class GaussianBlur final: public RenderToTexture {
public:
	GaussianBlur(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format, UINT _blurCount);
	~GaussianBlur() override = default;
	void InitShader(const std::wstring& binaryName0, const std::wstring& binaryName2);
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void InitRootSignature();
	// backBuffer变换状态为copySource; m_resource变换状态为copyDesc,最终变换为generic read
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE gpuDesc, UINT descSize);
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void InitTexture();

	ID3D12Resource* GetResourceDownSampler() const;
	ID3D12Resource* GetResourceUpSampler() const;
private:
	void CreateDescriptors() override;
	void CreateResources() override;
	void SetUnorderedAccess(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* blurMap) const;
	void SetGenericRead(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* blurMap) const;
private:
	static constexpr UINT			MAX_BLUR_TIMES = 5U;
	// RWStructuredBuffer -> CreateUnorderedAccessView
	// 此外还有一种名为原始缓冲区的资源，使用字节数组表示数据，可以通过字节偏移量找到所需数据的位置，再做强制类型转换
	ComPtr<ID3D12Resource>			m_resource1;
	ComPtr<ID3D12PipelineState>		m_pso1;
	ComPtr<ID3D12RootSignature>		m_signature;

	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuUAV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuUAV1;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuUAV1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuSRV1;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuSRV1;
	// Compute Shader中的资源计算完毕后可能需要将资源回传到后台内存缓冲区中

	std::unique_ptr<TexSizeChange>  m_DownUp;
	std::unique_ptr<Shader>			m_shader;
	std::unique_ptr<Shader>			m_shader1;

	std::array<float, 4>			m_weights;
	std::array<float, 4>			m_offsets;
	UINT							m_shrinkWidth;
	UINT							m_shrinkHeight;
	float							m_invHeight;
	float							m_invWidth;
	UINT							srvOffset;
	UINT							m_blurCount;
};
}

