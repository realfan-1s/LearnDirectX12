#include "TileBasedDefer.h"
#include "RtvDsvMgr.h"
#include "Texture.h"
#include <DirectXColors.h>

using namespace Renderer;

TileBasedDefer::TileBasedDefer(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: IRenderer(_device, _width, _height, _format),
m_pointLightUploader(std::make_unique<UploaderBuffer<LightInCompute>>(_device, pointLightNum, false))
{
	m_rtvOffset = RtvDsvMgr::instance().RegisterRTV(2);
	CreateResources();
}

void TileBasedDefer::InitRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE gBufferTable;
	gBufferTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE outputTable;
	outputTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 1, 0);
	CD3DX12_ROOT_PARAMETER parameters[5]{};
	parameters[0].InitAsConstantBufferView(0);
	parameters[1].InitAsDescriptorTable(1, &gBufferTable);
	parameters[2].InitAsDescriptorTable(1, &outputTable);
	parameters[3].InitAsShaderResourceView(0);
	parameters[4].InitAsConstants(4, 1, 0);

	auto sampler = GetStaticSampler();
	// 组成根签名
	CD3DX12_ROOT_SIGNATURE_DESC rootDesc(5U, parameters, sampler.size(), sampler.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> serializeRootSig{ nullptr };
	ComPtr<ID3DBlob> error{ nullptr };
	auto res = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializeRootSig.GetAddressOf(), error.GetAddressOf());
	if (error)
	{
		OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
	}
	ThrowIfFailed(res);
	ThrowIfFailed(m_device->CreateRootSignature(0, serializeRootSig->GetBufferPointer(), serializeRootSig->GetBufferSize(), IID_PPV_ARGS(m_pointRootSig.GetAddressOf())));
}

void TileBasedDefer::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC dirDesc = templateDesc;
	dirDesc.InputLayout = { m_shaderPack[L"Shaders\\Box"]->GetInputLayouts(),
	m_shaderPack[L"Shaders\\Box"]->GetInputLayoutSize() };
	dirDesc.VS = { static_cast<BYTE*>(m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	dirDesc.PS = { static_cast<BYTE*>(m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),
	m_shaderPack[L"Shaders\\Box"]->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	dirDesc.DepthStencilState.DepthEnable = false;
	dirDesc.NumRenderTargets = 2U;
	dirDesc.RTVFormats[0] = m_format;
	dirDesc.RTVFormats[1] = m_format;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dirDesc, IID_PPV_ARGS(&m_pso)));

	D3D12_COMPUTE_PIPELINE_STATE_DESC pointDesc{};
	pointDesc.CS = { m_shaderPack[L"Shaders\\TileBased_Defer"]->GetShaderByType(ShaderPos::compute)->GetBufferPointer(), m_shaderPack[L"Shaders\\TileBased_Defer"]->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	pointDesc.pRootSignature = m_pointRootSig.Get();
	pointDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	pointDesc.NodeMask = 0;
	ThrowIfFailed(m_device->CreateComputePipelineState(&pointDesc, IID_PPV_ARGS(&m_pointPso)));
}

void TileBasedDefer::InitTexture()
{
	m_bloomIdx = TextureMgr::instance().RegisterRenderToTexture("Bloom0");
	TextureMgr::instance().RegisterRenderToTexture("Bloom1");
	TextureMgr::instance().RegisterRenderToTexture("Bloom0UAV");
	TextureMgr::instance().RegisterRenderToTexture("Bloom1UAV");
}

void TileBasedDefer::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart,
D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, UINT srvSize, UINT rtvSize, UINT dsvSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandler(srvCpuStart, static_cast<INT>(m_bloomIdx), srvSize);
	m_cpuSRV = srvCpuHandler;
	m_bloomCpuSRV = srvCpuHandler.Offset(1, srvSize);
	m_bloomCpuUAV[0] = srvCpuHandler.Offset(1, srvSize);
	m_bloomCpuUAV[1] = srvCpuHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandler(srvGpuStart, static_cast<INT>(m_bloomIdx), srvSize);
	m_gpuSRV = srvGpuHandler;
	m_bloomGpuSRV = srvGpuHandler.Offset(1, srvSize);
	m_bloomGpuUAV[0] = srvGpuHandler.Offset(1, srvSize);
	m_bloomGpuUAV[1] = srvGpuHandler.Offset(1, srvSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuHandler(rtvCpuStart, static_cast<INT>(m_rtvOffset), rtvSize);
	m_bloomRTV[0] = rtvCpuHandler;
	m_bloomRTV[1] = rtvCpuHandler.Offset(1, rtvSize);

	InitDSV(dsvCpuStart, dsvSize);
	CreateDescriptors();
}

void TileBasedDefer::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc)
{

}

void TileBasedDefer::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const
{
	cmdList->ClearRenderTargetView(m_bloomRTV[0], Colors::LightSteelBlue, 0, nullptr);
	cmdList->ClearRenderTargetView(m_bloomRTV[1], Colors::LightSteelBlue, 0, nullptr);
	cmdList->OMSetRenderTargets(2, &m_bloomRTV[0], true, &m_cpuDSV);
	cmdList->SetPipelineState(m_pso.Get());
	cmdList->SetName(L"Direct Light Draw");
	DrawCanvas(cmdList);
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, m_bloomRes.Get());

	cmdList->SetComputeRootSignature(m_pointRootSig.Get());
	cmdList->SetPipelineState(m_pointPso.Get());
	cmdList->SetName(L"Point Light Draw");
	// DrawFunc需要传入gBuffer和cbPass
	drawFunc(NULL);
	constexpr UINT debug[] = { true, false, false, false };
	cmdList->SetComputeRoot32BitConstants(4U, 4, &debug, 0);
	cmdList->SetComputeRootDescriptorTable(2, m_bloomGpuUAV[0]);
	cmdList->SetComputeRootShaderResourceView(3, m_pointLightUploader->GetResource()->GetGPUVirtualAddress());
	const UINT groupX = static_cast<UINT>(std::ceilf(static_cast<float>(m_width) / 16.0f));
	const UINT groupY = static_cast<UINT>(std::ceilf(static_cast<float>(m_height) / 16.0f));
	cmdList->Dispatch(groupX, groupY, 1);
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_bloomRes.Get());
}

void TileBasedDefer::UpdatePointLights(const vector<std::shared_ptr<Light<Compute>>>& points)
{
	for (UINT i = 0; i < pointLightNum; ++i)
	{
		LightInCompute light = points[i]->GetData();
		m_pointLightUploader->Copy(i, light);
	}
}

auto TileBasedDefer::GetStaticSampler() -> std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> const
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2, D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 8);

	return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}

void TileBasedDefer::InitDSV(D3D12_CPU_DESCRIPTOR_HANDLE _cpuDSV, UINT dsvSize)
{
	m_cpuDSV = _cpuDSV;
}

void TileBasedDefer::CreateDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MostDetailedMip = 0;
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	m_device->CreateShaderResourceView(m_bloomRes.Get(), &srvDesc, m_bloomCpuSRV);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = m_format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_bloomRTV[0]);
	m_device->CreateRenderTargetView(m_bloomRes.Get(), &rtvDesc, m_bloomRTV[1]);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = m_format;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateUnorderedAccessView(m_resource.Get(), nullptr, &uavDesc, m_bloomCpuUAV[0]);
	m_device->CreateUnorderedAccessView(m_bloomRes.Get(), nullptr, &uavDesc, m_bloomCpuUAV[1]);
}

void TileBasedDefer::CreateResources()
{
	D3D12_RESOURCE_DESC resDesc{};
	ZeroMemory(&resDesc, sizeof(D3D12_RESOURCE_DESC));
	resDesc.Width = m_width;
	resDesc.Height = m_height;
	resDesc.Format = m_format;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Alignment = 0;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	resDesc.MipLevels = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_resource)));
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_bloomRes)));
	}
}
