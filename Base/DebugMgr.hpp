#pragma once

#include <d3d12.h>
#include <Src/d3dx12.h>
#include <wrl/client.h>
#include <array>

#include "D3DUtil.hpp"
#include "Shader.h"
#include "Singleton.hpp"

namespace Debug
{
class DebugMgr : public Singleton<DebugMgr>
{
public:
	explicit DebugMgr(typename Singleton<DebugMgr>::Token) : Singleton<DebugMgr>() {}
	DebugMgr(const DebugMgr&) = delete;
	DebugMgr& operator=(const DebugMgr&) = delete;
	DebugMgr(DebugMgr&&) = delete;
	DebugMgr& operator=(DebugMgr&&) = delete;
	~DebugMgr() override = default;

	void InitRequiredParameters(ID3D12Device* _device);
	const ID3D12RootSignature* GetRootSignature() const;
	const ID3D12PipelineState* GetPso() const;
	void PrepareToDebug(ID3D12GraphicsCommandList* cmdList, const D3D12_CPU_DESCRIPTOR_HANDLE& handler) const;
private:
	auto GetStaticSampler()->std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> const;
	void InitSignature();
	void InitShader();
	void InitPso();
private:
	template <typename  T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12Device>		m_device;
	ComPtr<ID3D12PipelineState>	m_pso;
	std::unique_ptr<Shader>		m_shader;
};

inline void DebugMgr::InitRequiredParameters(ID3D12Device* _device)
{
	m_device = _device;
	InitSignature();
	InitShader();
	InitPso();
}

inline const ID3D12RootSignature* DebugMgr::GetRootSignature() const
{
	return m_rootSignature.Get();
}

inline const ID3D12PipelineState* DebugMgr::GetPso() const
{
	return m_pso.Get();
}

inline void DebugMgr::PrepareToDebug(ID3D12GraphicsCommandList* cmdList, const D3D12_CPU_DESCRIPTOR_HANDLE& handler) const
{
	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
	cmdList->OMSetRenderTargets(1, &handler, true, nullptr);
	cmdList->SetPipelineState(m_pso.Get());
}

inline auto DebugMgr::GetStaticSampler()->std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> const
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2, D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 8);

	return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}

inline void DebugMgr::InitSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_ROOT_PARAMETER parameters{};
	parameters.InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_PIXEL);
	auto sampler = GetStaticSampler();
	//组成根签名
	const CD3DX12_ROOT_SIGNATURE_DESC rootDesc(1U, &parameters, sampler.size(), sampler.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
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

inline void DebugMgr::InitShader()
{
	m_shader = std::make_unique<Shader>(default_shader, L"Shaders\\DebugShading", initializer_list<D3D12_INPUT_ELEMENT_DESC>({
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		}));
}

inline void DebugMgr::InitPso()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	psoDesc.VS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	psoDesc.PS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1u;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1u;
	psoDesc.SampleDesc.Quality = 0u;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS((&m_pso))));
}
}