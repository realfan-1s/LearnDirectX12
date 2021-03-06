#include "DynamicCubeMap.h"
#include <DirectXColors.h>
#include "RtvDsvMgr.h"
#include "Texture.h"

Effect::DynamicCubeMap::DynamicCubeMap(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: RenderToTexture(_device, _width, _height, _format)
{
	m_passOffset = PassConstant::RegisterPassCount(6);
	m_rtvOffset = RtvDsvMgr::instance().RegisterRTV(6);
	m_dsvOffset = RtvDsvMgr::instance().RegisterDSV(1);
	CreateResources();
}

void Effect::DynamicCubeMap::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT srvSize, UINT rtvSize, UINT dsvSize)
{
	InitDSV(dsvCpuStart, dsvSize);
	INT srvIndex = static_cast<UINT>(GetSrvIdx("CubeMap").value_or(0));
	m_cpuSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, srvIndex, srvSize);
	m_gpuSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, srvIndex, srvSize);
	for (int i = 0 ; i < 6; ++i)
	{
		m_cpuRtv[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, i + m_rtvOffset , rtvSize);
	}
	CreateDescriptors();
	InitDepthAndStencil();
}

void Effect::DynamicCubeMap::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) {
	for (UINT i = 0; i < 6; ++i)
	{
		PassConstant passCB;
		XMStoreFloat4x4(&passCB.view_gpu, XMMatrixTranspose(m_cams[i].GetCurrViewXM()));
		XMStoreFloat4x4(&passCB.proj_gpu, XMMatrixTranspose(m_cams[i].GetCurrProjXM()));
		XMStoreFloat4x4(&passCB.vp_gpu, XMMatrixTranspose(m_cams[i].GetCurrVPXM()));
		XMStoreFloat3(&passCB.cameraPos_gpu, m_cams[i].GetCurrPosXM());
		passCB.deltaTime_gpu = static_cast<float>(timer.DeltaTime());
		passCB.totalTime_gpu = static_cast<float>(timer.TotalTime());
		passCB.nearZ_gpu = m_viewport.MaxDepth;
		passCB.farZ_gpu = m_viewport.MinDepth;
		passCB.renderTargetSize_gpu = { static_cast<float>(m_width), static_cast<float>(m_height) };
		passCB.invRenderTargetSize_gpu = { 1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height) };
		updateFunc(m_passOffset + i, passCB);
	}
}

void Effect::DynamicCubeMap::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
	cmdList->RSSetViewports(1, &m_viewport);
	cmdList->RSSetScissorRects(1, &m_scissorRect);
	// ????????????????????RENDER_TARGET
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_resource.Get());
	for (UINT i = 0U; i < 6U; ++i)
	{
		// ??????????????????????????
		cmdList->ClearRenderTargetView(m_cpuRtv[i], Colors::LightSteelBlue, 0, nullptr);
		cmdList->ClearDepthStencilView(m_cpuDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
		// ????????????????????
		cmdList->OMSetRenderTargets(1, &m_cpuRtv[i], true, &m_cpuDSV);
		// ??????????????????????????????????
		drawFunc(i);
	}

	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_resource.Get());
}

void Effect::DynamicCubeMap::InitTexture(std::string_view name)
{
	m_srvOffset = TextureMgr::instance().RegisterRenderToTexture(name);
}

void Effect::DynamicCubeMap::InitShader(const std::wstring& binaryName) {
	m_shader = make_unique<Shader>(default_shader, binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC>({ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} }));
}

void Effect::DynamicCubeMap::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC dynamicDesc = templateDesc;
	dynamicDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	dynamicDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	dynamicDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	dynamicDesc.VS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	dynamicDesc.PS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dynamicDesc, IID_PPV_ARGS(&m_pso)));
}

void Effect::DynamicCubeMap::InitCamera(float x, float y, float z)
{
	XMFLOAT3 center(x, y, z);
	// ????????????????????????????
	XMFLOAT3 target[6] = {
		XMFLOAT3(x + 1.0f, y, z),	XMFLOAT3(x - 1.0f, y, z),
		XMFLOAT3(x, y + 1.0f, z),	XMFLOAT3(x, y - 1.0f, z),
		XMFLOAT3(x, y, z + 1.0f),	XMFLOAT3(x, y, z - 1.0f)
	};

	// ??+y??-y??????????????+y??-y??????????????????????????y????
	XMFLOAT3 up[6] = {
		XMFLOAT3(0.0f, 1.0f, 0.0f),		XMFLOAT3(0.0f, 1.0f, 0.0f),
		XMFLOAT3(0.0f, 0.0f, -1.0f),	XMFLOAT3(0.0f, 0.0f, 1.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f),		XMFLOAT3(0.0f, 1.0f, 0.0f)
	};

	for (int i = 0; i < 6; ++i)
	{
		m_cams[i].LookAt(center, target[i], up[i]);
		m_cams[i].SetFrustumReverseZ(0.5f * XM_PI, 1.0f, 0.1f, 1000.0f);
		m_cams[i].Update();
	}
}

void Effect::DynamicCubeMap::InitDSV(D3D12_CPU_DESCRIPTOR_HANDLE cpuDsvStart, UINT dsvSize) {
	m_cpuDSV = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuDsvStart, (INT)m_dsvOffset, dsvSize);
}

void Effect::DynamicCubeMap::InitDepthAndStencil() {
	D3D12_RESOURCE_DESC dsDesc;
	dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.Alignment = 0;
	dsDesc.Width = m_width;
	dsDesc.Height = m_height;
	dsDesc.DepthOrArraySize = 1;
	dsDesc.MipLevels = 1;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 0.0f;
	optClear.DepthStencil.Stencil = 0;
	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &dsDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear, IID_PPV_ARGS(m_depthStencilRes.GetAddressOf())));
	}

	m_device->CreateDepthStencilView(m_depthStencilRes.Get(), nullptr, m_cpuDSV);
}

// ??dynamic cube map??????????????
void Effect::DynamicCubeMap::CreateDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Format = m_format;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	// ????????????????srv
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	// ??????????????????RTV
	for (int i = 0; i < 6; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = m_format;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
		// ????i??????????????????
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		// ??????????????????????
		rtvDesc.Texture2DArray.ArraySize = 1;
		m_device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_cpuRtv[i]);
	}
}

void Effect::DynamicCubeMap::CreateResources() {
	SetViewPortAndScissor();
	D3D12_RESOURCE_DESC resDesc;
	ZeroMemory(&resDesc, sizeof(D3D12_RESOURCE_DESC));
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Alignment = 0;
	resDesc.Width = m_width;
	resDesc.Height = m_height;
	// ??????????????
	resDesc.DepthOrArraySize = 6;
	resDesc.MipLevels = 1;
	resDesc.Format = m_format;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_resource)));
	}
}
