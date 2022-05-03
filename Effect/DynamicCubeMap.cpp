#include "DynamicCubeMap.h"
#include <DirectXColors.h>

Effect::DynamicCubeMap::DynamicCubeMap(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: RenderToTexture(_device, _width, _height, _format)
{
	CreateResources();
}

void Effect::DynamicCubeMap::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart, UINT rtvOffset, UINT srvSize, UINT rtvSize)
{
	INT srvIndex = static_cast<UINT>(GetSrvIdx("CubeMap").value_or(0));
	m_cpuSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, srvIndex, srvSize);
	m_gpuSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, srvIndex, srvSize);
	for (int i = 0 ; i < 6; ++i)
	{
		m_cpuRtv[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, i + rtvOffset, rtvSize);
	}
	CreateDescriptors();
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
		passCB.nearZ_gpu = m_viewport.MinDepth;
		passCB.farZ_gpu = m_viewport.MaxDepth;
		passCB.renderTargetSize_gpu = { static_cast<float>(m_width), static_cast<float>(m_height) };
		passCB.invRenderTargetSize_gpu = { 1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height) };
		updateFunc(i, passCB);
	}
}

void Effect::DynamicCubeMap::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
	cmdList->RSSetViewports(1, &m_viewport);
	cmdList->RSSetScissorRects(1, &m_scissorRect);
	// ��������ͼ��Դת��ΪRENDER_TARGET
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, m_resource.Get());
	for (UINT i = 0U; i < 6U; ++i)
	{
		// �����̨����������Ȼ�����
		cmdList->ClearRenderTargetView(m_cpuRtv[i], Colors::LightSteelBlue, 0, nullptr);
		cmdList->ClearDepthStencilView(m_cpuDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		// ָ����Ҫ��Ⱦ�Ļ�����
		cmdList->OMSetRenderTargets(1, &m_cpuRtv[i], true, &m_cpuDSV);
		// Ϊ��ǰ��������󶨳���������������
		drawFunc(i);
	}

	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_resource.Get());
}

void Effect::DynamicCubeMap::InitShader(const std::wstring& binaryName) {
	m_shader = make_unique<Shader>(default_shader, binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC>({ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} }));
}

void Effect::DynamicCubeMap::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC dynamicDesc = templateDesc;
	dynamicDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	dynamicDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	dynamicDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	dynamicDesc.VS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	dynamicDesc.PS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&dynamicDesc, IID_PPV_ARGS(&m_pso)));
}

void Effect::DynamicCubeMap::InitCamera(float x, float y, float z)
{
	XMFLOAT3 center(x, y, z);
	// ����ÿ��������ķ�����й۲�
	XMFLOAT3 target[6] = {
		XMFLOAT3(x + 1.0f, y, z),	XMFLOAT3(x - 1.0f, y, z),
		XMFLOAT3(x, y + 1.0f, z),	XMFLOAT3(x, y - 1.0f, z),
		XMFLOAT3(x, y, z + 1.0f),	XMFLOAT3(x, y, z - 1.0f)
	};

	// ��+y��-y��������Ҫ����+y��-y�۲죬�����Ҫһ������ͬ��y����
	XMFLOAT3 up[6] = {
		XMFLOAT3(0.0f, 1.0f, 0.0f),		XMFLOAT3(0.0f, 1.0f, 0.0f),
		XMFLOAT3(0.0f, 0.0f, -1.0f),	XMFLOAT3(0.0f, 0.0f, 1.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f),		XMFLOAT3(0.0f, 1.0f, 0.0f)
	};

	for (int i = 0; i < 6; ++i)
	{
		m_cams[i].LookAt(center, target[i], up[i]);
		m_cams[i].SetFrustum(0.5f * XM_PI, 1.0f, 0.1f, 1000.0f);
		m_cams[i].Update();
	}
}

// Ϊdynamic cube map��������������
void Effect::DynamicCubeMap::CreateDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Format = m_format;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	// ���������������srv
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	// Ϊÿ���������洴��RTV
	for (int i = 0; i < 6; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = m_format;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
		// Ϊ��i��Ԫ�ش�����Ⱦ��ͼ
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		// ÿ��Ԫ��ֻ����һ����ͼ
		rtvDesc.Texture2DArray.ArraySize = 1;
		m_device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_cpuRtv[i]);
	}
}

void Effect::DynamicCubeMap::CreateResources() {
	D3D12_RESOURCE_DESC resDesc;
	ZeroMemory(&resDesc, sizeof(D3D12_RESOURCE_DESC));
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Alignment = 0;
	resDesc.Width = m_width;
	resDesc.Height = m_height;
	// ������������Դ
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
