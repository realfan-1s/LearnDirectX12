#pragma once
#include "RenderToTexture.h"

namespace Effect
{
class TexSizeChange final : public RenderToTexture {
private:
	struct Sampler;
public:
	struct DownSampler;
	struct UpSampler;
public:
	TexSizeChange(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format, UINT _shrinkScale);
	TexSizeChange(const TexSizeChange&) = delete;
	TexSizeChange& operator=(const TexSizeChange&) = delete;
	TexSizeChange(TexSizeChange&&) = default;
	TexSizeChange& operator=(TexSizeChange&&) = default;
	~TexSizeChange() override = default;

	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE gpuDesc, UINT descSize);
	void InitShader();
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
	template <typename T, std::enable_if_t<std::is_base_of_v<Sampler, T> && T::value>* = nullptr>
	void SubDraw(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* source) const;
	template <typename T, std::enable_if_t<std::is_base_of_v<Sampler, T> && !T::value>* = nullptr>
	void SubDraw(ID3D12GraphicsCommandList* cmdList, CD3DX12_GPU_DESCRIPTOR_HANDLE sourceSRV) const;
	void InitTexture();
	void InitRootSignature();

	ID3D12Resource* GetDownSamplerResource() const;
	ID3D12Resource* GetUpSamplerResource() const;
	std::tuple<UINT, UINT, float, float> GetShrinkTuple() const;
private:
	struct Sampler {};
	struct alignas(16) SizeData {
		float	m_invRawWidth;
		float	m_invRawHeight;
		float	m_invShrinkWidth;
		float	m_invShrinkHeight;
		UINT	m_shrinkWidth;
		UINT	m_shrinkHeight;
	} m_sizeData;
	std::unique_ptr<Shader>			downSampler;
	ComPtr<ID3D12Resource>			downSamplerRes;
	ComPtr<ID3D12Resource>			downSamplerRes1;
	ComPtr<ID3D12PipelineState>		downSamplerPso;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuDownSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuDownSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuDownUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuDownUAV;
	UINT							downSRVIdx;

	std::unique_ptr<Shader>			upSampler;
	ComPtr<ID3D12Resource>			upSamplerRes;
	ComPtr<ID3D12PipelineState>		upSamplerPso;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuUpUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuUpUAV;
	UINT							upSRVIdx;

	ComPtr<ID3D12RootSignature>		m_signature;
private:
	void CreateResources() override;
	void CreateDescriptors() override;
	SizeData ReCalcSize() const;
public:
	struct DownSampler : public Sampler, public std::true_type{};
	struct UpSampler : public Sampler, public std::false_type{};
};

template <typename T, std::enable_if_t<std::is_base_of_v<TexSizeChange::Sampler, T> && T::value>*>
void TexSizeChange::SubDraw(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* source) const
{
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(downSamplerRes1.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->ResourceBarrier(1, &trans);
	}
	cmdList->SetPipelineState(downSamplerPso.Get());
	cmdList->SetComputeRootDescriptorTable(1, m_gpuDownSRV);
	cmdList->SetComputeRootDescriptorTable(2, m_gpuDownUAV);
	const UINT downGroupX = static_cast<UINT>(std::ceilf(static_cast<float>(m_sizeData.m_shrinkWidth) / 16.0f));
	const UINT downGroupY = static_cast<UINT>(std::ceilf(static_cast<float>(m_sizeData.m_shrinkHeight) / 16.0f));
	cmdList->Dispatch(downGroupX, downGroupY, 1);
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(downSamplerRes.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON);
		cmdList->ResourceBarrier(1, &trans);
	}
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(downSamplerRes1.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		cmdList->ResourceBarrier(1, &trans);
	}
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(1, &trans);
	}
	cmdList->CopyResource(source, downSamplerRes1.Get());
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &trans);
	}
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(downSamplerRes1.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
		cmdList->ResourceBarrier(1, &trans);
	}
}

template <typename T, std::enable_if_t<std::is_base_of_v<TexSizeChange::Sampler, T> && !T::value>*>
void TexSizeChange::SubDraw(ID3D12GraphicsCommandList* cmdList, CD3DX12_GPU_DESCRIPTOR_HANDLE sourceSRV) const
{
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(upSamplerRes.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->ResourceBarrier(1, &trans);
	}
	cmdList->SetPipelineState(upSamplerPso.Get());
	cmdList->SetComputeRootDescriptorTable(1, sourceSRV);
	cmdList->SetComputeRootDescriptorTable(2, m_gpuUpUAV);
	const UINT upGroupX = static_cast<UINT>(std::ceilf(static_cast<float>(m_width) / 16.0f));
	const UINT upGroupY = static_cast<UINT>(std::ceilf(static_cast<float>(m_height) / 16.0f));
	cmdList->Dispatch(upGroupX, upGroupY, 1);
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(upSamplerRes.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
		cmdList->ResourceBarrier(1, &trans);
	}
}
}

