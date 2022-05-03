#include "ToneMap.h"
#include "PostProcessMgr.hpp"
#include "Texture.h"

Effect::ToneMap::ToneMap(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: RenderToTexture(_device, _width, _height, _format){
	CreateResources();
}

Effect::ToneMap::~ToneMap() = default;

void Effect::ToneMap::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_COMPUTE_PIPELINE_STATE_DESC toneDesc {};
	toneDesc.CS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::compute)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	toneDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	toneDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&toneDesc, IID_PPV_ARGS(&m_pso)));
}

void Effect::ToneMap::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) {
}

void Effect::ToneMap::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, bloomRes[0].Get());
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, m_resource.Get());
	cmdList->SetComputeRootSignature(PostProcessMgr::instance().GetRootSignature());
	cmdList->SetPipelineState(m_pso.Get());
	cmdList->SetComputeRootDescriptorTable(1, m_gpuSRV);
	cmdList->SetComputeRootDescriptorTable(2, toneMapUAV_GPU);
	drawFunc(NULL);
	UINT groupX = (UINT)std::ceilf((float)m_width / 16.0f);
	UINT groupY = (UINT)std::ceilf((float)m_height / 16.0f);
	cmdList->Dispatch(groupX, groupY, 1);
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE>(cmdList, m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, bloomRes[0].Get());
}

void Effect::ToneMap::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvStart, UINT rtvOffset, UINT rtvSize, UINT srvSize) {
	auto rtvBloom = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStart, 6 + (INT)rtvOffset, rtvSize);
	bloomRTV[0] = rtvBloom;
	bloomRTV[1] = rtvBloom.Offset(1, rtvSize);
	auto bloomSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (INT)bloomIdx, srvSize);
	m_cpuSRV = bloomSRV;
	bloomCpuSRV = bloomSRV.Offset(1, srvSize);
	auto bloomSRV_GPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, (INT)bloomIdx, srvSize);
	m_gpuSRV = bloomSRV_GPU;
	bloomGpuSRV = bloomSRV_GPU.Offset(1, srvSize);

	toneMapUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (INT)toneMapIdx, srvSize);
	toneMapUAV_GPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, (INT)toneMapIdx, srvSize);

	CreateDescriptors();
}

void Effect::ToneMap::InitShader(const wstring& binaryName) {
	m_shader = std::make_unique<Shader>(compute_shader, binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC>());
}

void Effect::ToneMap::InitTexture() {
	TextureMgr::instance().RegisterRenderToTexture("Bloom0");
	TextureMgr::instance().RegisterRenderToTexture("Bloom1");
	bloomIdx = TextureMgr::instance().GetRegisterType("Bloom0").value();
	TextureMgr::instance().RegisterRenderToTexture("ToneMap");
	toneMapIdx = TextureMgr::instance().GetRegisterType("ToneMap").value();
}

void Effect::ToneMap::CreateResources() {
	D3D12_RESOURCE_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(D3D12_RESOURCE_DESC));
	rtvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rtvDesc.Alignment = 0;
	rtvDesc.Width = m_width;
	rtvDesc.Height = m_height;
	rtvDesc.DepthOrArraySize = 1;
	rtvDesc.MipLevels = 1;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.SampleDesc.Count = 1;
	rtvDesc.SampleDesc.Quality = 0;
	rtvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rtvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &rtvDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&bloomRes[0])));
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &rtvDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&bloomRes[1])));
	}

	D3D12_RESOURCE_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(D3D12_RESOURCE_DESC));
	uavDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	uavDesc.Alignment = 0;
	uavDesc.Width = m_width;
	uavDesc.Height = m_height;
	uavDesc.Alignment = 0;
	rtvDesc.DepthOrArraySize = 1;
	rtvDesc.MipLevels = 1;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.SampleDesc.Count = 1;
	rtvDesc.SampleDesc.Quality = 0;
	rtvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rtvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &rtvDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));
	}
}

void Effect::ToneMap::CreateDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	// 创建整个渲染器MRT的srv
	m_device->CreateShaderResourceView(bloomRes[0].Get(), &srvDesc, m_cpuSRV);
	m_device->CreateShaderResourceView(bloomRes[1].Get(), &srvDesc, bloomCpuSRV);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = m_format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateRenderTargetView(bloomRes[0].Get(), &rtvDesc, bloomRTV[0]);
	m_device->CreateRenderTargetView(bloomRes[1].Get(), &rtvDesc, bloomRTV[1]);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	m_device->CreateUnorderedAccessView(m_resource.Get(), nullptr, &uavDesc, toneMapUAV);
}
