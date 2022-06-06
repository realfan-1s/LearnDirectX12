#include "SSAO.h"
#include "D3DUtil.hpp"
#include "Texture.h"
#include "RtvDsvMgr.h"
#include <DirectXColors.h>
#include <DirectXPackedVector.h>

using namespace Effect;
using namespace DirectX::PackedVector;

Effect::SSAO::SSAO(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: RenderToTexture(_device, _width, _height, _format), m_ssaoUploader(std::make_unique<UploaderBuffer<SSAOPass>>(_device, 1, true)), m_bilateralBlur(std::make_unique<BilateralBlur<blurByCompute>>(_device, _width, _height, _format, 1u)){
	rtvIdx = RtvDsvMgr::instance().RegisterRTV(1);
	CreateResources();
}

void Effect::SSAO::OnResize(UINT newWidth, UINT newHeight) {
	if (m_width != newWidth || m_height != newHeight)
	{
		m_width = newWidth;
		m_height = newHeight;
		CreateResources();
		CreateDescriptors();
		m_bilateralBlur->OnResize(newWidth, newHeight);
	}
}

void Effect::SSAO::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoDesc = templateDesc;
	ssaoDesc.DepthStencilState.DepthEnable = false;
	ssaoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoDesc.InputLayout = { ssaoShader->GetInputLayouts(), ssaoShader->GetInputLayoutSize() };
	ssaoDesc.VS = { static_cast<BYTE*>(ssaoShader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), ssaoShader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	ssaoDesc.PS = { static_cast<BYTE*>(ssaoShader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()), ssaoShader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	ssaoDesc.NumRenderTargets = 1U;
	ssaoDesc.RTVFormats[0] = m_format;
	ssaoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&ssaoDesc, IID_PPV_ARGS(&m_pso)));

	m_bilateralBlur->InitPSO(templateDesc);
}

void Effect::SSAO::InitTexture() {
	resIdx = TextureMgr::instance().RegisterRenderToTexture("SSAO");
	randomIdx = TextureMgr::instance().RegisterRenderToTexture("RandomTex");
	m_bilateralBlur->InitTexture("ssaoDown", "ssaoUp", "ssaoHorizontal", "ssaoVertical");
}

void Effect::SSAO::InitShader() {
	ssaoShader = std::make_unique<Shader>(default_shader, L"Shaders\\SSAO", initializer_list<D3D12_INPUT_ELEMENT_DESC>());
	m_bilateralBlur->InitShader();
}

void Effect::SSAO::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) {
}

void Effect::SSAO::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_resource.Get());
	cmdList->ClearRenderTargetView(m_cpuRTV, Colors::White, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &m_cpuRTV, true, nullptr);
	auto randHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	randHandler.Offset(randomIdx, m_srvSize);
	cmdList->SetGraphicsRootDescriptorTable(8, randHandler);
	cmdList->SetPipelineState(m_pso.Get());
	const auto ssaoRes = m_ssaoUploader->GetResource()->GetGPUVirtualAddress();
	cmdList->SetGraphicsRootConstantBufferView(9, ssaoRes);
	cmdList->SetName(L"SSAO Draw");
	DrawCanvas(cmdList);

	m_bilateralBlur->Draw(cmdList, [&](UINT){
		ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE>(cmdList, m_resource.Get());
		cmdList->CopyResource(m_bilateralBlur->GetDownResource(), m_resource.Get());
	});

	ChangeState<D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE>(cmdList, m_bilateralBlur->GetUpResource());
	cmdList->CopyResource(m_resource.Get(), m_bilateralBlur->GetUpResource());
	ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON>(cmdList, m_bilateralBlur->GetUpResource());
}

void Effect::SSAO::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuSRVStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSRVStart, D3D12_CPU_DESCRIPTOR_HANDLE cpuRTVStart, UINT srvSize, UINT rtvSize) {
	m_srvSize = srvSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSRVHandler(cpuSRVStart, static_cast<INT>(resIdx), srvSize);
	m_cpuSRV = cpuSRVHandler;
	randomCpuSRV = cpuSRVHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSRVHandler(gpuSRVStart, static_cast<INT>(resIdx), srvSize);
	m_gpuSRV = gpuSRVHandler;
	randomGpuSRV = gpuSRVHandler.Offset(1, srvSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuRTVHandler(cpuRTVStart, static_cast<INT>(rtvIdx), rtvSize);
	m_cpuRTV = cpuRTVHandler;

	CreateDescriptors();
	m_bilateralBlur->CreateDescriptors(cpuSRVStart, gpuSRVStart, srvSize);
}

void Effect::SSAO::CreateDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
	m_device->CreateShaderResourceView(randomTex.Get(), &srvDesc, randomCpuSRV);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = m_format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_cpuRTV);
}

void Effect::SSAO::CreateResources()
{
	D3D12_RESOURCE_DESC resDesc;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = m_format;
	resDesc.Width = m_width;
	resDesc.Height = m_height;
	resDesc.MipLevels = 1;
	resDesc.Alignment = 0;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	{
		const auto properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		constexpr float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const CD3DX12_CLEAR_VALUE optClear(m_format, clearColor);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&m_resource)));
		m_resource->SetName(L"SSAOOutput");
	}
}

void Effect::SSAO::CreateRandomTexture(ID3D12GraphicsCommandList* cmdList) {
	// 法向半球随机转动更适合
	constexpr UINT halfCircle = 256U;
	D3D12_RESOURCE_DESC resDesc;
	ZeroMemory(&resDesc, sizeof(D3D12_RESOURCE_DESC));
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
	resDesc.Alignment = 0;
	resDesc.Width = halfCircle;
	resDesc.Height = halfCircle;
	resDesc.MipLevels = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&randomTex)));
		randomTex->SetName(L"SSAORandomTex");
	}

	// 为了将CPU内存数据复制到默认缓冲区域，需要创建一个中间默认上传堆
	UINT num2DRes = resDesc.DepthOrArraySize * resDesc.MipLevels;
	{
		UINT64 uploadBufferSize = GetRequiredIntermediateSize(randomTex.Get(), 0, num2DRes);
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto& buffer = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(randomUploader.GetAddressOf())));
	}

	std::vector<XMCOLOR> initData;
	initData.reserve(halfCircle * halfCircle);
#pragma omp parallel for
	for (int x = 0; x < halfCircle; ++x)
		for (int y = 0; y < halfCircle; ++y)
		{
			initData.emplace_back(XMCOLOR{MathHelper::MathHelper::Rand(), MathHelper::MathHelper::Rand(), MathHelper::MathHelper::Rand(), 0.0f});
		}
	D3D12_SUBRESOURCE_DATA subresData{};
	subresData.pData = initData.data();
	subresData.RowPitch = 64 * sizeof(XMCOLOR);
	subresData.SlicePitch = subresData.RowPitch * 64;
	// 将数据复制到默认资源中并更改状态
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, randomTex.Get());
	UpdateSubresources(cmdList, randomTex.Get(), randomUploader.Get(), 0, 0, num2DRes, &subresData);
	ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, randomTex.Get());

	// 采样十四个均匀分布的向量实现SSAO
	SSAOPass passCB;
	passCB.offset[0] = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
	passCB.offset[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);
	passCB.offset[2] = XMFLOAT4(-1.0f, 1.0f, 1.0f, 0.0f);
	passCB.offset[3] = XMFLOAT4(1.0f, -1.0f, -1.0f, 0.0f);
	passCB.offset[4] = XMFLOAT4(1.0f, 1.0f, -1.0f, 0.0f);
	passCB.offset[5] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);
	passCB.offset[6] = XMFLOAT4(-1.0f, 1.0f, -1.0f, 0.0f);
	passCB.offset[7] = XMFLOAT4(1.0f, -1.0f, 1.0f, 0.0f);
	// 六个面中心点
	passCB.offset[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	passCB.offset[9] = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	passCB.offset[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	passCB.offset[11] = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	passCB.offset[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	passCB.offset[13] = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	passCB.radius = 0.5f;
	passCB.attenuationEnd = 1.0f;
	passCB.attenuationStart = 0.2f;
	passCB.surfaceEpsilon = 0.05f;
	m_ssaoUploader->Copy(0, passCB);
}
