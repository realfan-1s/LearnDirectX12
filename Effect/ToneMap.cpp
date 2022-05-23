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
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, m_resource.Get());
	cmdList->SetComputeRootSignature(PostProcessMgr::instance().GetRootSignature());
	cmdList->SetPipelineState(m_pso.Get());
	cmdList->SetComputeRootDescriptorTable(2, toneMapUAV_GPU);
	drawFunc(NULL);
	UINT groupX = (UINT)std::ceilf((float)m_width / 16.0f);
	UINT groupY = (UINT)std::ceilf((float)m_height / 16.0f);
	cmdList->Dispatch(groupX, groupY, 1);
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE>(cmdList, m_resource.Get());
}

void Effect::ToneMap::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, UINT srvSize) {
	toneMapUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (INT)toneMapIdx, srvSize);
	toneMapUAV_GPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, (INT)toneMapIdx, srvSize);

	CreateDescriptors();
}

void Effect::ToneMap::InitShader(const wstring& binaryName) {
	m_shader = std::make_unique<Shader>(compute_shader, binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC>());
}

void Effect::ToneMap::InitTexture() {
	toneMapIdx = TextureMgr::instance().RegisterRenderToTexture("ToneMap");
}

void Effect::ToneMap::CreateResources() {
	D3D12_RESOURCE_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(D3D12_RESOURCE_DESC));
	uavDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	uavDesc.Alignment = 0;
	uavDesc.Width = m_width;
	uavDesc.Height = m_height;
	uavDesc.Alignment = 0;
	uavDesc.DepthOrArraySize = 1;
	uavDesc.MipLevels = 1;
	uavDesc.Format = m_format;
	uavDesc.SampleDesc.Count = 1;
	uavDesc.SampleDesc.Quality = 0;
	uavDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	uavDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));
		m_resource->SetName(L"ToneMapOutput");
	}
}

void Effect::ToneMap::CreateDescriptors() {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = m_format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	m_device->CreateUnorderedAccessView(m_resource.Get(), nullptr, &uavDesc, toneMapUAV);
}
