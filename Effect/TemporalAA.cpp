#include "TemporalAA.h"
#include <iostream>
#include <DirectXColors.h>
#include "Texture.h"
#include "PostProcessMgr.hpp"
#include "RtvDsvMgr.h"

using namespace Effect;

TemporalAA::TemporalAA(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: RenderToTexture(_device, _width, _height, _format),
m_motionVector(std::make_unique<MotionVector>(_device, _width, _height, DXGI_FORMAT_R16G16_SNORM))
{
	prevRtvIdx = RtvDsvMgr::instance().RegisterRTV(1);
	CreateResources();
}

void TemporalAA::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	psoDesc.CS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::compute)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
	m_motionVector->InitPSO(templateDesc);
}

void TemporalAA::InitShader(const wstring& taaName, const wstring& motionVecName)
{
	m_shader = std::make_unique<Shader>(compute_shader, taaName, initializer_list<D3D12_INPUT_ELEMENT_DESC>());
	m_motionVector->InitShader(motionVecName);
}

void TemporalAA::InitTexture(const string& temporalName, const string& prevName, const string& motionVecName)
{
	TAASrvIdx = TextureMgr::instance().RegisterRenderToTexture(temporalName);
	TAAUavIdx = TextureMgr::instance().RegisterRenderToTexture(temporalName + "UAV");
	prevSrvIdx = TextureMgr::instance().RegisterRenderToTexture(prevName);
	prevUavIdx = TextureMgr::instance().RegisterRenderToTexture(prevName + "UAV");
	m_motionVector->InitTexture(motionVecName);
}

void TemporalAA::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc)
{
	XMFLOAT2 currJitter = GetJitter();
	XMFLOAT2 prevJitter = GetPrevJitter();
	m_motionVector->SetJitter(prevJitter, currJitter);
}

void TemporalAA::OnResize(UINT newWidth, UINT newHeight)
{
	if (m_width != newWidth || m_height != newHeight)
	{
		m_width = newWidth;
		m_height = newHeight;
		CreateResources();
		CreateDescriptors();
		m_motionVector->OnResize(newWidth, newHeight);
		m_dirtyFlag = true;
	}
}

void TemporalAA::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const
{
	m_motionVector->Draw(cmdList, [](UINT){});

	cmdList->SetComputeRootSignature(PostProcessMgr::instance().GetRootSignature());
	cmdList->SetPipelineState(m_pso.Get());
	auto jitters = GetJitter();
	const float parameters[] = { static_cast<float>(m_width), static_cast<float>(m_height),
								1.0f / static_cast<float>(m_width) , 1.0f / static_cast<float>(m_height),
								jitters.x, jitters.y, 0.95f, 0.98f, 0.85f };
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, m_resource.Get());
	cmdList->SetComputeRoot32BitConstants(0, 9, &parameters, 0);
	drawFunc(NULL); 
	cmdList->SetComputeRootDescriptorTable(2, m_gpuUAV);
	cmdList->SetComputeRootDescriptorTable(3, m_prevGpuSRV);
	cmdList->SetComputeRootDescriptorTable(6, GetMotionVector());

	const UINT groupX = static_cast<UINT>(std::ceilf(static_cast<float>(m_width) / 16.0f));
	const UINT groupY = static_cast<UINT>(std::ceilf(static_cast<float>(m_height) / 16.0f));
	cmdList->SetName(L"TemporalAA");
	cmdList->Dispatch(groupX, groupY, 1);

	// 将当前的resource拷贝到prevResource中去
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, m_prevResource.Get());
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE>(cmdList, m_resource.Get());
	cmdList->CopyResource(m_prevResource.Get(), m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_prevResource.Get());
	ChangeState<D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_resource.Get());

	// jitterPoint偏移
	if (++jitterPivot >= 8)
	{
		jitterPivot = 0;
	}
}

void TemporalAA::FirstDraw(ID3D12GraphicsCommandList* cmdList, const D3D12_CPU_DESCRIPTOR_HANDLE& depthHandler, const std::function<void()>& drawFunc) const
{
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_prevResource.Get());
	cmdList->ClearRenderTargetView(m_prevCpuRTV, Colors::Black, 0, nullptr);
	cmdList->ClearDepthStencilView(depthHandler, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &m_prevCpuRTV, true, &depthHandler);
	drawFunc();
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_prevResource.Get());
	// jitterPoint偏移
	if (++jitterPivot >= 8)
	{
		jitterPivot = 0;
	}
}

void TemporalAA::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT srvSize, UINT rtvSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandler(srvCpuStart, static_cast<INT>(TAASrvIdx), srvSize);
	m_cpuSRV = srvCpuHandler;
	m_cpuUAV = srvCpuHandler.Offset(1, srvSize);
	m_prevCpuSRV = srvCpuHandler.Offset(1, srvSize);
	m_prevCpuUAV = srvCpuHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandler(srvGpuStart, static_cast<INT>(TAASrvIdx), srvSize);
	m_gpuSRV = srvGpuHandler;
	m_gpuUAV = srvGpuHandler.Offset(1, srvSize);
	m_prevGpuSRV = srvGpuHandler.Offset(1, srvSize);
	m_prevGpuUAV = srvGpuHandler.Offset(1, srvSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuHandler(rtvCpuStart, static_cast<INT>(prevRtvIdx), rtvSize);
	m_prevCpuRTV = rtvCpuHandler;

	CreateDescriptors();

	m_motionVector->CreateDescriptors(srvCpuStart, srvGpuStart, rtvCpuStart, srvSize, rtvSize);
}

XMFLOAT2 TemporalAA::GetJitter() const
{
	auto& num = MathHelper::HaltonSequence<8>().value[jitterPivot];
	XMFLOAT2 ans = XMFLOAT2((num.x * 2.0f - 1.0f) / m_width, (num.y * 2.0f - 1.0f) / m_height);
	return ans;
}

XMFLOAT2 TemporalAA::GetPrevJitter() const
{
	UINT prevPivot = (jitterPivot + 7) % 8;
	auto& num = MathHelper::HaltonSequence<8>().value[prevPivot];
	XMFLOAT2 ans = XMFLOAT2((num.x * 2.0f - 1.0f) / m_width, (num.y * 2.0f - 1.0f) / m_height);
	return ans;
}

D3D12_GPU_DESCRIPTOR_HANDLE TemporalAA::GetMotionVector() const
{
	return m_motionVector->GetGpuSRV();
}

const D3D12_CPU_DESCRIPTOR_HANDLE& TemporalAA::GetPrevRTV() const
{
	return m_prevCpuRTV;
}

void TemporalAA::CreateResources()
{
	D3D12_RESOURCE_DESC resDesc;
	ZeroMemory(&resDesc, sizeof(D3D12_RESOURCE_DESC));
	resDesc.Format = m_format;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.DepthOrArraySize = 1;
	resDesc.Alignment = 0;
	resDesc.Width = m_width;
	resDesc.Height = m_height;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_resource)));
		m_resource->SetName(L"outputTAA");

		constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		CD3DX12_CLEAR_VALUE optClear(m_format, clearColor);
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&m_prevResource)));
		m_prevResource->SetName(L"previousFrame");
	}
}

void TemporalAA::CreateDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = m_format;
	uavDesc.Texture2D.MipSlice = 0;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = m_format;
	rtvDesc.Texture2D.MipSlice = 0;

	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	m_device->CreateUnorderedAccessView(m_resource.Get(), nullptr, &uavDesc, m_cpuUAV);
	m_device->CreateShaderResourceView(m_prevResource.Get(), &srvDesc, m_prevCpuSRV);
	m_device->CreateUnorderedAccessView(m_prevResource.Get(), nullptr, &uavDesc, m_prevCpuUAV);
	m_device->CreateRenderTargetView(m_prevResource.Get(), &rtvDesc, m_prevCpuRTV);
}
