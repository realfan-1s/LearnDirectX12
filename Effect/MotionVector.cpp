#include "MotionVector.h"
#include "Texture.h"
#include "PostProcessMgr.hpp"
#include "RtvDsvMgr.h"
#include <DirectXColors.h>

using namespace Effect;

MotionVector::MotionVector(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: RenderToTexture(_device, _width, _height, _format)
{
	motionVectorRTVIdx = RtvDsvMgr::instance().RegisterRTV(1);
	CreateResources();
}

void MotionVector::InitTexture(const string& _name)
{
	motionVectorSRVIdx = TextureMgr::instance().RegisterRenderToTexture(_name);
}

void Effect::MotionVector::InitShader(const wstring& motionVecName)
{
	m_shader = std::make_unique<Shader>(default_shader, motionVecName, initializer_list<D3D12_INPUT_ELEMENT_DESC>());
}

void MotionVector::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = templateDesc;
	psoDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	psoDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	psoDesc.VS = { m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer(), m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	psoDesc.PS = { m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer(), m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.NumRenderTargets = 1U;
	psoDesc.RTVFormats[0] = m_format;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void MotionVector::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT srvSize, UINT rtvSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandler(srvCpuStart, static_cast<INT>(motionVectorSRVIdx), srvSize);
	m_cpuSRV = srvCpuHandler;
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandler(srvGpuStart, static_cast<INT>(motionVectorSRVIdx), srvSize);
	m_gpuSRV = srvGpuHandler;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuHandler(rtvCpuStart, static_cast<INT>(motionVectorRTVIdx), rtvSize);
	m_cpuRTV = rtvCpuHandler;

	CreateDescriptors();
}

void Effect::MotionVector::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc)
{
}

void Effect::MotionVector::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const
{
	cmdList->SetGraphicsRootSignature(PostProcessMgr::instance().GetRootSignature());
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_resource.Get());
	cmdList->ClearRenderTargetView(m_cpuRTV, Colors::Black, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &m_cpuRTV, true, nullptr);
	cmdList->SetPipelineState(m_pso.Get());
	cmdList->SetName(L"MotionVector");
	DrawCanvas(cmdList);
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_resource.Get());
}

void MotionVector::CreateDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = m_format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = m_format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_cpuRTV);
}

void MotionVector::CreateResources()
{
	D3D12_RESOURCE_DESC resDesc;
	ZeroMemory(&resDesc, sizeof(D3D12_RESOURCE_DESC));
	resDesc.Width = m_width;
	resDesc.Height = m_height;
	resDesc.Format = m_format;
	resDesc.Alignment = 0;
	resDesc.MipLevels = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	{
		const auto properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		CD3DX12_CLEAR_VALUE optClear(m_format, clearColor);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&m_resource)));
		m_resource->SetName(L"MotionVector");
	}
}
