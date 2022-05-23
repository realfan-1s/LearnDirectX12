#include "GuassianBlur.h"
#include "PostProcessMgr.hpp"
#include "Texture.h"

Effect::GaussianBlur::GaussianBlur(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format, UINT _blurCount)
: RenderToTexture(_device, _width, _height, _format), m_DownUp(std::make_unique<TexSizeChange>(_device, _width, _height, _format, 4)), m_blurCount(std::clamp(_blurCount, 1U, MAX_BLUR_TIMES))
{
	auto [weights, offsets] = MathHelper::MathHelper::CalcBilinearGaussian<7>();
	m_weights = weights;
	m_offsets = offsets;
	CreateResources();
}

void Effect::GaussianBlur::InitShader(const std::wstring& binaryName, const std::wstring& binaryName1) {
	m_shader = std::make_unique<Shader>(ShaderType::compute_shader, binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC>());
	m_shader1 = std::make_unique<Shader>(ShaderType::compute_shader, binaryName1, initializer_list<D3D12_INPUT_ELEMENT_DESC>());
	m_DownUp->InitShader();
}

void Effect::GaussianBlur::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_COMPUTE_PIPELINE_STATE_DESC blurDesc{};
	blurDesc.CS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::compute)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	blurDesc.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	blurDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&blurDesc, IID_PPV_ARGS(&m_pso)));

	D3D12_COMPUTE_PIPELINE_STATE_DESC blurDesc1{};
	blurDesc1.CS = { static_cast<BYTE*>(m_shader1->GetShaderByType(ShaderPos::compute)->GetBufferPointer()), m_shader1->GetShaderByType(ShaderPos::compute)->GetBufferSize() };
	blurDesc1.pRootSignature = PostProcessMgr::instance().GetRootSignature();
	blurDesc1.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateComputePipelineState(&blurDesc1, IID_PPV_ARGS(&m_pso1)));
	m_DownUp->InitPSO(templateDesc);
}

void Effect::GaussianBlur::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE gpuDesc, UINT descSize) {
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandler(cpuDesc, static_cast<INT>(srvOffset), descSize);
	m_cpuSRV = cpuHandler;
	m_cpuUAV = cpuHandler.Offset(1, descSize);
	m_cpuSRV1 = cpuHandler.Offset(1, descSize);
	m_cpuUAV1 = cpuHandler.Offset(1, descSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandler(gpuDesc, static_cast<INT>(srvOffset), descSize);
	m_gpuSRV = gpuHandler;
	m_gpuUAV = gpuHandler.Offset(1, descSize);
	m_gpuSRV1 = gpuHandler.Offset(1, descSize);
	m_gpuUAV1 = gpuHandler.Offset(1, descSize);

	CreateDescriptors();

	m_DownUp->CreateDescriptors(cpuDesc, gpuDesc, descSize);
}

void Effect::GaussianBlur::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, GetResourceDownSampler());
	drawFunc(NULL);
	ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, GetResourceDownSampler());
	m_DownUp->SubDraw<TexSizeChange::DownSampler>(cmdList, m_resource.Get());

	cmdList->SetComputeRootSignature(PostProcessMgr::instance().GetRootSignature());
	float texSize[4] = { static_cast<float>(m_shrinkWidth), static_cast<float>(m_shrinkHeight),
		m_invWidth, m_invHeight };
	cmdList->SetComputeRoot32BitConstants(0, 4, &texSize, 0);
	cmdList->SetComputeRoot32BitConstants(0, m_weights.size(), m_weights.data(), 4);
	cmdList->SetComputeRoot32BitConstants(0, m_offsets.size(), m_offsets.data(), 4 + m_weights.size());

	// 开始进行真正地模糊操作
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, m_resource1.Get());
	cmdList->SetName(L"GaussianBlur");
	for (UINT i = 0; i < m_blurCount; ++i)
	{
		// Horizontal blur
		const UINT horizontalGroupX = static_cast<UINT>(std::ceilf(static_cast<float>(m_shrinkWidth) / 256.0f));
		cmdList->SetPipelineState(m_pso.Get());
		cmdList->SetComputeRootDescriptorTable(1, m_gpuSRV);
		cmdList->SetComputeRootDescriptorTable(2, m_gpuUAV1);
		cmdList->Dispatch(horizontalGroupX, m_shrinkHeight, 1);

		SetGenericRead(cmdList, m_resource1.Get());
		SetUnorderedAccess(cmdList, m_resource.Get());

		// Vertical Blur
		const UINT verticalGroupY = static_cast<UINT>(std::ceilf(static_cast<float>(m_shrinkHeight) / 256.0f));
		cmdList->SetPipelineState(m_pso1.Get());
		cmdList->SetComputeRootDescriptorTable(1, m_gpuSRV1);
		cmdList->SetComputeRootDescriptorTable(2, m_gpuUAV);
		cmdList->Dispatch(m_shrinkWidth, verticalGroupY, 1);

		SetGenericRead(cmdList, m_resource.Get());
		SetUnorderedAccess(cmdList, m_resource1.Get());
	}

	/*
	 * 每个线程组都被指定了一个线程组ID，系统值语义为SV_GroupID;
	 * 每个线程组内的线程都有一个组内ID，系统值语义为SV_GroupThreadID;
	 * 单个线程在全局线程组内的位置，系统值语义为SV_dispatchID
	 */
	/*
	 * 共享内存与线程同步：每一个线程都有自己的共享内存，需要通过SV_GroupThreadID对其进行索引，令组内的每个线程都访问共享内存中的同一个元素
	 * 但可能会产生线程同步问题和性能问题
	*/
	m_DownUp->SubDraw<TexSizeChange::UpSampler>(cmdList, m_gpuSRV);

	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON>(cmdList, m_resource.Get());
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON>(cmdList, m_resource1.Get());
}

void Effect::GaussianBlur::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) {
	if (m_dirtyFlag)
	{
		m_DownUp->OnResize(m_width, m_height);
		m_dirtyFlag = false;
	}
}

void Effect::GaussianBlur::InitTexture(const string& horizontal, const string& vertical, const string& down, const string& up) {
	InitSRV(horizontal);
	srvOffset = TextureMgr::instance().GetRegisterType(horizontal).value();
	InitSRV(horizontal + "UAV");
	InitSRV(vertical);
	InitSRV(vertical + "UAV");

	m_DownUp->InitTexture(down, up);
}

ID3D12Resource* Effect::GaussianBlur::GetResourceDownSampler() const {
	return m_DownUp->GetDownSamplerResource();
}

ID3D12Resource* Effect::GaussianBlur::GetResourceUpSampler() const
{
	return m_DownUp->GetUpSamplerResource();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Effect::GaussianBlur::GetUpSamplerSRV() const
{
	return m_DownUp->GetUpSamplerSRV();
}

void Effect::GaussianBlur::CreateDescriptors() {
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

	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	m_device->CreateUnorderedAccessView(m_resource.Get(), nullptr, &uavDesc, m_cpuUAV); // pCounterResource是与UAV相关联的计数器
	m_device->CreateShaderResourceView(m_resource1.Get(), &srvDesc, m_cpuSRV1);
	m_device->CreateUnorderedAccessView(m_resource1.Get(), nullptr, &uavDesc, m_cpuUAV1);
}

void Effect::GaussianBlur::CreateResources() {
	m_shrinkWidth = static_cast<UINT>(std::ceilf(static_cast<float>(m_width) / 4.0f));
	m_shrinkHeight = static_cast<UINT>(std::ceilf(static_cast<float>(m_height) / 4.0f));
	m_invWidth = 1.0f / static_cast<float>(m_shrinkWidth);
	m_invHeight = 1.0f / static_cast<float>(m_shrinkHeight);

	// 创建资源描述符
	D3D12_RESOURCE_DESC resDesc;
	ZeroMemory(&resDesc, sizeof(D3D12_RESOURCE_DESC));
	resDesc.Format = m_format;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.DepthOrArraySize = 1;
	resDesc.Alignment = 0;
	resDesc.Width = m_shrinkWidth;
	resDesc.Height = m_shrinkHeight;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	  
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource1)));
	}
}

void Effect::GaussianBlur::SetUnorderedAccess(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* blurMap) const {
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>(cmdList, blurMap);
}

void Effect::GaussianBlur::SetGenericRead(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* blurMap) const {
	ChangeState<D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, blurMap);
}
