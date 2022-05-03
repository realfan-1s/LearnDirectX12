#include "TexSizeChange.h"
#include "PostProcessMgr.hpp"
#include "Texture.h"

using namespace  Effect;

Effect::TexSizeChange::TexSizeChange(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format, UINT _shrinkScale)
: RenderToTexture(_device, _width, _height, _format)
{
	CreateResources();
}

void Effect::TexSizeChange::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
}

void Effect::TexSizeChange::CreateResources() {
	m_sizeData = ReCalcSize();
	D3D12_RESOURCE_DESC downDesc;
	ZeroMemory(&downDesc, sizeof(D3D12_RESOURCE_DESC));
	downDesc.Format = m_format;
	downDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	downDesc.DepthOrArraySize = 1;
	downDesc.Alignment = 0;
	downDesc.Width = m_sizeData.m_shrinkWidth;
	downDesc.Height = m_sizeData.m_shrinkHeight;
	downDesc.SampleDesc.Count = 1;
	downDesc.SampleDesc.Quality = 0;
	downDesc.MipLevels = 1;
	downDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	downDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_DESC upDesc;
	ZeroMemory(&upDesc, sizeof(D3D12_RESOURCE_DESC));
	upDesc.Format = m_format;
	upDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	upDesc.DepthOrArraySize = 1;
	upDesc.Alignment = 0;
	upDesc.Width = m_width;
	upDesc.Height = m_height;
	upDesc.SampleDesc.Count = 1;
	upDesc.SampleDesc.Quality = 0;
	upDesc.MipLevels = 1;
	upDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	upDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &upDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&downSamplerRes)));
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &downDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&downSamplerRes1)));
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &upDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&upSamplerRes)));
	}
}

void Effect::TexSizeChange::CreateDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = m_format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	m_device->CreateShaderResourceView(downSamplerRes.Get(), &srvDesc, m_cpuDownSRV);
	m_device->CreateUnorderedAccessView(downSamplerRes1.Get(), nullptr, &uavDesc, m_cpuDownUAV); // pCounterResource是与UAV相关联的计数器
	m_device->CreateUnorderedAccessView(upSamplerRes.Get(), nullptr, &uavDesc, m_cpuUpUAV);
}

TexSizeChange::SizeData TexSizeChange::ReCalcSize() const
{
	UINT shrinkWidth = static_cast<UINT>(std::ceilf(static_cast<float>(m_width) / 4.0f));
	UINT shrinkHeight = static_cast<UINT>(std::ceilf(static_cast<float>(m_height) / 4.0f));
	return { 1.0f / m_width, 1.0f / m_height, 1.0f / shrinkWidth, 1.0f / shrinkHeight, shrinkWidth, shrinkHeight };
}

void Effect::TexSizeChange::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE gpuDesc, UINT descSize) {
	CD3DX12_CPU_DESCRIPTOR_HANDLE downCpuHandler(cpuDesc, static_cast<INT>(downSRVIdx), descSize);
	m_cpuDownSRV = downCpuHandler;
	m_cpuDownUAV = downCpuHandler.Offset(1, descSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE downGpuHandler(gpuDesc, static_cast<INT>(downSRVIdx), descSize);
	m_gpuDownSRV = downGpuHandler;
	m_gpuDownUAV = downGpuHandler.Offset(1, descSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE upCpuHandler(cpuDesc, static_cast<INT>(upSRVIdx), descSize);
	m_cpuUpUAV = upCpuHandler;
	CD3DX12_GPU_DESCRIPTOR_HANDLE upGpuHandler(gpuDesc, static_cast<INT>(upSRVIdx), descSize);
	m_gpuUpUAV = upGpuHandler;

	CreateDescriptors();
}

void Effect::TexSizeChange::InitShader() {
	downSampler = std::make_unique<Shader>(ShaderType::compute_shader, L"Shaders\\DownSampler_Down", initializer_list<D3D12_INPUT_ELEMENT_DESC>());
	upSampler = std::make_unique<Shader>(ShaderType::compute_shader, L"Shaders\\DownSampler_Up", initializer_list<D3D12_INPUT_ELEMENT_DESC>());
}

void Effect::TexSizeChange::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_COMPUTE_PIPELINE_STATE_DESC downDesc{};
	downDesc.CS = { static_cast<BYTE*>(downSampler->GetShaderByType(ShaderPos::compute)->GetBufferPointer()), downSampler->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	downDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	downDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&downDesc, IID_PPV_ARGS(&downSamplerPso)));
	D3D12_COMPUTE_PIPELINE_STATE_DESC upDesc{};
	upDesc.CS = { static_cast<BYTE*>(upSampler->GetShaderByType(ShaderPos::compute)->GetBufferPointer()), upSampler->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	upDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	upDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&upDesc, IID_PPV_ARGS(&upSamplerPso)));
}

void Effect::TexSizeChange::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) {
}

void Effect::TexSizeChange::InitTexture() {
	InitSRV("downSampler");
	InitSRV("downSampler1");
	InitSRV("upSampler");
	downSRVIdx = TextureMgr::instance().GetRegisterType("downSampler").value();
	upSRVIdx = TextureMgr::instance().GetRegisterType("upSampler").value();
}

void TexSizeChange::InitRootSignature()
{
	m_signature = PostProcessMgr::instance().GetRootSignature();
}

ID3D12Resource* Effect::TexSizeChange::GetDownSamplerResource() const {
	return downSamplerRes.Get();
}

ID3D12Resource* Effect::TexSizeChange::GetUpSamplerResource() const {
	return upSamplerRes.Get();
}

std::tuple<UINT, UINT, float, float> Effect::TexSizeChange::GetShrinkTuple() const {
	return std::make_tuple(m_sizeData.m_shrinkWidth, m_sizeData.m_shrinkHeight, m_sizeData.m_invShrinkWidth, m_sizeData.m_invShrinkHeight);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE TexSizeChange::GetUpSamplerSRV() const
{
	return m_gpuUpUAV;
}
