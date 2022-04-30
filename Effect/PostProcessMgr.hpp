#pragma once

#include "Singleton.hpp"
#include <wrl/client.h>
#include <d3d12.h>
#include <array>
#include "D3DUtil.hpp"

class PostProcessMgr :public Singleton<PostProcessMgr> {
public:
	explicit PostProcessMgr(typename Singleton<PostProcessMgr>::Token) : Singleton<PostProcessMgr>() {}
	PostProcessMgr(const PostProcessMgr&) = delete;
	PostProcessMgr(PostProcessMgr&&) = delete;
	PostProcessMgr& operator=(const PostProcessMgr&) = delete;
	PostProcessMgr& operator=(PostProcessMgr&&) = delete;
	~PostProcessMgr() override = default;

	ID3D12RootSignature* GetRootSignature() const;
	void Init(ID3D12Device* _device)
	{
		if (m_device != nullptr)
			return;
		m_device = _device;
		InitRootSignature();
	}
	void Init(const ComPtr<ID3D12Device>& _device)
	{
		if (m_device != nullptr)
			return;
		m_device = _device;
		InitRootSignature();
	}
private:
	void InitRootSignature();
	auto GetStaticSampler()->std::array<CD3DX12_STATIC_SAMPLER_DESC, 6> const;
private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12Device>		m_device;
};

inline void PostProcessMgr::InitRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	// 创建根参数
	CD3DX12_ROOT_PARAMETER parameters[3]{};
	parameters[0].InitAsConstants(12, 0);
	parameters[1].InitAsDescriptorTable(1, &srvTable);
	parameters[2].InitAsDescriptorTable(1, &uavTable);

	auto sampler = GetStaticSampler();
	//组成根签名
	CD3DX12_ROOT_SIGNATURE_DESC rootDesc(3U, parameters, sampler.size(), sampler.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> serializeRootSig{ nullptr };
	ComPtr<ID3DBlob> error{ nullptr };
	auto res = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializeRootSig.GetAddressOf(), error.GetAddressOf());
	if (error)
	{
		OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
	}
	ThrowIfFailed(res);
	ThrowIfFailed(m_device->CreateRootSignature(0, serializeRootSig->GetBufferPointer(), serializeRootSig->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}

inline ID3D12RootSignature* PostProcessMgr::GetRootSignature() const
{
	return m_rootSignature.Get();
}

inline auto PostProcessMgr::GetStaticSampler()->std::array<CD3DX12_STATIC_SAMPLER_DESC, 6> const
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2, D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 8);

	return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}
