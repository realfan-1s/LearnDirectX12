#pragma once

#include "RenderToTexture.h"
#include "RtvDsvMgr.h"
#include "Texture.h"
#include "TexSizeChange.h"
#include "UploaderBuffer.hpp"
#include "PostProcessMgr.hpp"
#include <array>
#include <DirectXColors.h>

namespace Effect
{
struct blurByCompute {};

struct blurByTrivial {};

template <typename T>
struct blurByType : public std::integral_constant<int, -1> {};

template <>
struct blurByType<blurByTrivial> : public std::integral_constant<int, 0> {};

template <>
struct blurByType<blurByCompute> : public std::integral_constant<int, 1> {};

template <typename T, typename U = int>
class BilateralBlur {};

// Trivial SHADER版本双边过滤
template <typename T>
class BilateralBlur<T, std::enable_if_t<blurByType<T>::value == 0, int>> final : public RenderToTexture {
public:
	BilateralBlur(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	BilateralBlur(BilateralBlur&) = delete;
	BilateralBlur& operator=(BilateralBlur&) = delete;
	BilateralBlur(BilateralBlur&&) = default;
	BilateralBlur& operator=(BilateralBlur&&) = default;
	~BilateralBlur() override = default;

	void InitShader();
	void InitTexture(const string& name = "SSAOBlur");
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuSRVStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSRVStart, D3D12_CPU_DESCRIPTOR_HANDLE cpuRTVStart, UINT srvSize, UINT rtvSize);
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;
private:
	void CreateBlurPass(bool horizontal) const;
	void CreateResources() override;
	void CreateDescriptors() override;
private:
	struct BlurPass {
		XMFLOAT4	weight0_gpu;
		XMFLOAT2	weight1_gpu;
		UINT		doHorizontal_gpu;
		UINT		objPad0_gpu;
	};
	ComPtr<ID3D12Resource>								horizontalRes;
	CD3DX12_CPU_DESCRIPTOR_HANDLE						horizontalCpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE						horizontalGpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE						m_cpuRTV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE						horizontalRTV;
	UINT												srvOffset;
	UINT												horizontalSrvIdx;
	UINT												rtvOffset;
	UINT												m_srvSize;
	std::unique_ptr<Shader>								m_shader;
	mutable std::unique_ptr<UploaderBuffer<BlurPass>>	blurUploader;
};

template <typename T>
BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::BilateralBlur
(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: RenderToTexture(_device, _width, _height, _format), blurUploader(std::make_unique<UploaderBuffer<BlurPass>>(_device, 2, true))
{
	CreateBlurPass(true);
	CreateBlurPass(false);
	rtvOffset = RtvDsvMgr::instance().RegisterRTV(2);
	CreateResources();
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::InitShader()
{
	m_shader = std::make_unique<Shader>(default_shader, L"Shaders\\BilateralBlur", initializer_list<D3D12_INPUT_ELEMENT_DESC>());
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::InitTexture(const string& name)
{
	srvOffset = TextureMgr::instance().RegisterRenderToTexture(name);
	horizontalSrvIdx = TextureMgr::instance().RegisterRenderToTexture(name + "Horizontal");
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC bilateralDesc = templateDesc;
	bilateralDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	bilateralDesc.VS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	bilateralDesc.PS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	bilateralDesc.DepthStencilState.DepthEnable = false;
	bilateralDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	bilateralDesc.NumRenderTargets = 1U;
	bilateralDesc.RTVFormats[0] = m_format;
	bilateralDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&bilateralDesc, IID_PPV_ARGS(&m_pso)));
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::CreateDescriptors(
	D3D12_CPU_DESCRIPTOR_HANDLE cpuSRVStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSRVStart,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuRTVStart, UINT srvSize, UINT rtvSize)
{
	m_srvSize = srvSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandler(cpuSRVStart, srvOffset, srvSize);
	m_cpuSRV = srvCpuHandler;
	horizontalCpuSRV = srvCpuHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandler(gpuSRVStart, srvOffset, srvSize);
	m_gpuSRV = srvGpuHandler;
	horizontalGpuSRV = srvGpuHandler.Offset(1, srvSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuHandler(cpuRTVStart, rtvOffset, srvSize);
	m_cpuRTV = rtvCpuHandler;
	horizontalRTV = rtvCpuHandler.Offset(1, rtvSize);

	CreateDescriptors();
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::Update
(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc)
{
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::Draw
(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const
{
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, horizontalRes.Get());
	cmdList->ClearRenderTargetView(horizontalRTV, Colors::White, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &horizontalRTV, true, nullptr);
	drawFunc(NULL); // 传入待模糊的纹理
	const auto blurByteSize = D3DUtil::AlignsConstantBuffer(sizeof(BlurPass));
	cmdList->SetGraphicsRootConstantBufferView(10, blurUploader->GetResource()->GetGPUVirtualAddress() + blurByteSize);
	cmdList->SetPipelineState(m_pso.Get());
	DrawCanvas(cmdList);
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, horizontalRes.Get());

	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_resource.Get());
	cmdList->ClearRenderTargetView(m_cpuRTV, Colors::White, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &m_cpuRTV, true, nullptr);
	cmdList->SetGraphicsRootConstantBufferView(10, blurUploader->GetResource()->GetGPUVirtualAddress() );
	auto horizontalHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	horizontalHandler.Offset(horizontalSrvIdx, m_srvSize);
	cmdList->SetGraphicsRootDescriptorTable(4, horizontalHandler);
	DrawCanvas(cmdList);
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON>(cmdList, m_resource.Get());
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::CreateResources()
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
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		constexpr float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const CD3DX12_CLEAR_VALUE optClear(m_format, clearColor);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, &optClear, IID_PPV_ARGS(&m_resource)));
		m_resource->SetName(L"BilateralBlurVertical");
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&horizontalRes)));
		horizontalRes->SetName(L"BilateralBlurHorizontal");
	}
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::CreateDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	m_device->CreateShaderResourceView(horizontalRes.Get(), &srvDesc, horizontalCpuSRV);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = m_format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_cpuRTV);
	m_device->CreateRenderTargetView(horizontalRes.Get(), &rtvDesc, horizontalRTV);
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 0, int>>::CreateBlurPass(bool horizontal) const
{
	BlurPass blurPass;
	auto blur = MathHelper::MathHelper::CalcGaussian<6>();
	blurPass.weight0_gpu = XMFLOAT4(blur[0], blur[1], blur[2], blur[3]);
	blurPass.weight1_gpu = XMFLOAT2(blur[4], blur[5]);
	blurPass.doHorizontal_gpu = static_cast<UINT>(horizontal);
	blurUploader->Copy(static_cast<INT>(horizontal), blurPass);
}

// COMPUTE SHADER版本双边过滤
template <typename T>
class BilateralBlur<T, std::enable_if_t<blurByType<T>::value == 1, int>> final : public RenderToTexture {
public:
	BilateralBlur(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format, UINT _blurCount);
	BilateralBlur(BilateralBlur&) = delete;
	BilateralBlur& operator=(BilateralBlur&) = delete;
	BilateralBlur(BilateralBlur&&) = default;
	BilateralBlur& operator=(BilateralBlur&&) = default;
	~BilateralBlur() override = default;

	void OnResize(UINT newWidth, UINT newHeight) override;
	void InitShader();
	void InitTexture(const string& down, const string& up, const string& horizontal, const string& vertical);
	void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) override;
	void CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvStart, UINT srvSize);
	void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) override;
	void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const override;

	ID3D12Resource* GetDownResource() const { return m_downUp->GetDownSamplerResource(); } 
	ID3D12Resource* GetUpResource() const { return m_downUp->GetUpSamplerResource(); }
private:
	void CreateResources() override;
	void CreateDescriptors() override;
private:
	ComPtr<ID3D12PipelineState>		m_verticalPso;
	ComPtr<ID3D12Resource>			downSamplerRes;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	downCpuUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	downGpuUAV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	downCpuSRV; // 进行升采样前的结果
	CD3DX12_GPU_DESCRIPTOR_HANDLE	downGpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuUAV;
	std::unique_ptr<Shader>			m_shader[2];
	std::unique_ptr<TexSizeChange>	m_downUp;

	constexpr static UINT			MAX_BLUR_TIMES = 5U;
	UINT							srvOffset;
	UINT							m_blurCount;
	UINT							shrinkWidth;
	UINT							shrinkHeight;
	float							invShrinkWidth;
	float							invShrinkHeight;
	array<float, 4>					m_weights;
	array<float, 4>					m_offsets;
};

template <typename T>
BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::BilateralBlur
(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format, UINT _blurCount)
: RenderToTexture(_device, _width, _height, _format), m_downUp(std::make_unique<TexSizeChange>(_device, _width, _height, _format, 4U)), m_blurCount(std::clamp(_blurCount, 1U, MAX_BLUR_TIMES)){
	auto [weights, offsets] = MathHelper::MathHelper::CalcBilinearGaussian<7>();
	m_weights = weights;
	m_offsets = offsets;
	CreateResources();
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::OnResize(UINT newWidth, UINT newHeight)
{
	if (m_width != newWidth || m_height != newHeight)
	{
		m_width = newWidth;
		m_height = newHeight;
		CreateResources();
		CreateDescriptors();

		m_downUp->OnResize(newWidth, newHeight);
	}
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::InitShader()
{
	m_shader[0] = std::make_unique<Shader>(compute_shader, L"Shaders\\BilateralBlur_Horizontal", initializer_list<D3D12_INPUT_ELEMENT_DESC>());
	m_shader[1] = std::make_unique<Shader>(compute_shader, L"Shaders\\BilateralBlur_Vertical", initializer_list<D3D12_INPUT_ELEMENT_DESC>());

	m_downUp->InitShader();
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::InitTexture(const string& down, const string& up, const string& horizontal, const string& vertical)
{
	srvOffset = TextureMgr::instance().RegisterRenderToTexture(horizontal); // down
	TextureMgr::instance().RegisterRenderToTexture(horizontal + "UAV"); // downUAV
	TextureMgr::instance().RegisterRenderToTexture(vertical); // resource
	TextureMgr::instance().RegisterRenderToTexture(vertical + "UAV"); // resourceUAV

	m_downUp->InitTexture(down, up);
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::InitPSO(
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC horizontalDesc{};
	horizontalDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	horizontalDesc.CS = { m_shader[0]->GetShaderByType(ShaderPos::compute)->GetBufferPointer(), m_shader[0]->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	horizontalDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&horizontalDesc, IID_PPV_ARGS(&m_pso)));

	D3D12_COMPUTE_PIPELINE_STATE_DESC verticalDesc{};
	verticalDesc.CS = { m_shader[1]->GetShaderByType(ShaderPos::compute)->GetBufferPointer(), m_shader[1]->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	verticalDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	verticalDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&verticalDesc, IID_PPV_ARGS(&m_verticalPso)));

	m_downUp->InitPSO(templateDesc);
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::CreateDescriptors(
	D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvStart, UINT srvSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandler(cpuSrvStart, srvOffset, srvSize);
	m_cpuSRV = srvCpuHandler;
	m_cpuUAV = srvCpuHandler.Offset(1, srvSize);
	downCpuSRV = srvCpuHandler.Offset(1, srvSize);;
	downCpuUAV = srvCpuHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandler(gpuSrvStart, srvOffset, srvSize);
	m_gpuSRV = srvGpuHandler;
	m_gpuUAV = srvGpuHandler.Offset(1, srvSize);
	downGpuSRV = srvGpuHandler.Offset(1, srvSize);;
	downGpuUAV = srvGpuHandler.Offset(1, srvSize);

	CreateDescriptors();

	m_downUp->CreateDescriptors(cpuSrvStart, gpuSrvStart, srvSize);
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::Update
(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc)
{
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::Draw(
	ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const
{
	// copyResources phase
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, GetDownResource());
	drawFunc(NULL);
	ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, GetDownResource());
	m_downUp->SubDraw<TexSizeChange::DownSampler>(cmdList, m_resource.Get());

	cmdList->SetComputeRootSignature(PostProcessMgr::instance().GetRootSignature());
	float texSize[4] = { static_cast<float>(shrinkWidth), static_cast<float>(shrinkHeight), invShrinkWidth, invShrinkHeight };
	cmdList->SetComputeRoot32BitConstants(0, 4, &texSize, 0);
	cmdList->SetComputeRoot32BitConstants(0, m_weights.size(), m_weights.data(), 4);
	cmdList->SetComputeRoot32BitConstants(0, m_offsets.size(), m_weights.data(), 4 + m_weights.size());

	// 开始进行真正的模糊
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, downSamplerRes.Get());
	cmdList->SetName(L"BilateralDraw");
	for (UINT i = 0; i < m_blurCount; ++i)
	{
		// Horizontal Blur
		const UINT horizontalGroupX = static_cast<UINT>(std::ceilf(static_cast<float>(shrinkWidth) / 256.0f));
		cmdList->SetPipelineState(m_pso.Get());
		cmdList->SetComputeRootDescriptorTable(1, m_gpuSRV);
		cmdList->SetComputeRootDescriptorTable(2, downGpuUAV);
		cmdList->Dispatch(horizontalGroupX, shrinkHeight, 1);

		ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, downSamplerRes.Get());
		ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, m_resource.Get());
		// Vertical Blur
		const UINT verticalGroupY = static_cast<UINT>(std::ceilf(static_cast<float>(shrinkHeight) / 256.0f));
		cmdList->SetPipelineState(m_verticalPso.Get());
		cmdList->SetComputeRootDescriptorTable(1, downGpuSRV);
		cmdList->SetComputeRootDescriptorTable(2, m_gpuUAV);
		cmdList->Dispatch(shrinkWidth, verticalGroupY, 1);

		ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, downSamplerRes.Get());
		ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_resource.Get());
	}

	m_downUp->SubDraw<TexSizeChange::UpSampler>(cmdList, m_gpuSRV);
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON>(cmdList, m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON>(cmdList, downSamplerRes.Get());
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::CreateResources()
{
	shrinkWidth = static_cast<UINT>(std::ceilf(static_cast<float>(m_width) / 4.0f));
	shrinkHeight = static_cast<UINT>(std::ceilf(static_cast<float>(m_height) / 4.0f));
	invShrinkWidth = 1.0f / static_cast<float>(shrinkWidth);
	invShrinkHeight = 1.0f / static_cast<float>(shrinkHeight);
	D3D12_RESOURCE_DESC resDesc;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = m_format;
	resDesc.Width = shrinkWidth;
	resDesc.Height = shrinkHeight;
	resDesc.MipLevels = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Alignment = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&downSamplerRes)));
	}
}

template <typename T>
void BilateralBlur<T, enable_if_t<blurByType<T>::value == 1, int>>::CreateDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	m_device->CreateShaderResourceView(downSamplerRes.Get(), &srvDesc, downCpuSRV);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = m_format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	m_device->CreateUnorderedAccessView(m_resource.Get(), nullptr, &uavDesc, m_cpuUAV);
	m_device->CreateUnorderedAccessView(downSamplerRes.Get(), nullptr, &uavDesc, downCpuUAV);
}
}

