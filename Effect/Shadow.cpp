#include "Shadow.h"
#include "D3DUtil.hpp"
#include "RtvDsvMgr.h"

Effect::Shadow::Shadow(ID3D12Device* _device, UINT _width) : RenderToTexture(_device, _width, _width, DXGI_FORMAT_R24G8_TYPELESS)
{
	m_dsvOffset = RtvDsvMgr::instance().RegisterDSV(1);
	CreateResources();
}

void Effect::Shadow::CreateDescriptors()
{
	// 创建SRV让shader能够采样shadow map
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);

	// 创建DSV让shader能够渲染到shadow map上
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Texture2D.MipSlice = 0;
	m_device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, m_cpuDSV);
}

void Effect::Shadow::CreateResources()
{
	SetViewPortAndScissor();

	D3D12_RESOURCE_DESC shadowDesc;
	ZeroMemory(&shadowDesc, sizeof(D3D12_RESOURCE_DESC));
	shadowDesc.Alignment = 0;
	shadowDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	shadowDesc.Width = m_width;
	shadowDesc.Height = m_width;
	shadowDesc.DepthOrArraySize = 1;
	shadowDesc.MipLevels = 1;
	shadowDesc.Format = m_format;
	shadowDesc.SampleDesc.Count = 1;
	shadowDesc.SampleDesc.Quality = 0;
	shadowDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	shadowDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 通过D3D12_CLEAR_VALUE在创建时填充这个渲染目标的纹理
	D3D12_CLEAR_VALUE shadowClear;
	shadowClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	shadowClear.DepthStencil.Depth = 0.0f;
	shadowClear.DepthStencil.Stencil = 0;

	{
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &shadowDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &shadowClear, IID_PPV_ARGS(m_resource.GetAddressOf())));
	}
}

void Effect::Shadow::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) {
	auto [lightViewXM, lightProjXM, lightPos, nearZ, farZ] = RegisterLightVPXM();
	// NDC空间变换到纹理空间
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);
	XMMATRIX lightVPXM = XMMatrixMultiply(lightViewXM, lightProjXM);
	m_shadowTransform = XMMatrixMultiply(lightVPXM, T);
	PassConstant shadowPass;
	XMStoreFloat4x4(&shadowPass.view_gpu, XMMatrixTranspose(std::move(lightViewXM)));
	XMStoreFloat4x4(&shadowPass.proj_gpu, XMMatrixTranspose(std::move(lightProjXM)));
	XMStoreFloat4x4(&shadowPass.vp_gpu, XMMatrixTranspose(std::move(lightVPXM)));
	XMStoreFloat4x4(&shadowPass.shadowTransform_gpu, XMMatrixTranspose(m_shadowTransform));
	XMStoreFloat3(&shadowPass.cameraPos_gpu, lightPos);
	shadowPass.nearZ_gpu = nearZ;
	shadowPass.farZ_gpu = farZ;
	shadowPass.renderTargetSize_gpu = std::move(XMFLOAT2(static_cast<float>(m_width), static_cast<float>(m_height)));
	shadowPass.invRenderTargetSize_gpu = std::move(XMFLOAT2(1.0f / static_cast<float>(m_width), 
		1.0f /static_cast<float>(m_height)));
	updateFunc(m_dsvOffset, shadowPass);
}

void Effect::Shadow::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const {
	cmdList->RSSetViewports(1, &m_viewport);
	cmdList->RSSetScissorRects(1, &m_scissorRect);
	// 为深度写入模式
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE>(cmdList, m_resource.Get());
	// 清除深度缓冲区和后台缓冲区
	cmdList->ClearDepthStencilView(m_cpuDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
	// 要将渲染目标设置为空,从而禁止颜色数据写入
	cmdList->OMSetRenderTargets(0, nullptr, false, &m_cpuDSV);
	cmdList->SetPipelineState(m_pso.Get());
	cmdList->SetName(L"Draw Shadow");
	drawFunc(0);

	// 结束深度写入状态
	ChangeState<D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_resource.Get());
}

void Effect::Shadow::InitShader(const std::wstring& binaryName) {
	m_shader = make_unique<Shader>(default_shader, binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC>({
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} }));
}


void Effect::Shadow::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowDesc = templateDesc;
	// 避免自遮挡的阴影偏移依赖于实际场景
	shadowDesc.RasterizerState.DepthBias = -100000;
	shadowDesc.RasterizerState.DepthBiasClamp = 0.0f;
	shadowDesc.RasterizerState.SlopeScaledDepthBias = -1.0f;
	shadowDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	shadowDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	shadowDesc.VS = {static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()),
	m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	shadowDesc.PS = {static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	// shadow map不需要render target
	shadowDesc.NumRenderTargets = 0;
	shadowDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&shadowDesc, IID_PPV_ARGS(&m_pso)));
}

void Effect::Shadow::RegisterMainLight(const Light* _mainLight) {
	m_mainLight = _mainLight;
}

const XMMATRIX& Effect::Shadow::GetShadowTransformXM() const
{
	return m_shadowTransform;
}

void Effect::Shadow::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart, D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuStart, UINT srvSize, UINT dsvSize) {
	INT srvIndex = static_cast<INT>(GetSrvIdx("ShadowMap").value_or(0));
	m_cpuSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, srvIndex, srvSize);
	m_gpuSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, srvIndex, srvSize);
	m_cpuDSV = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, m_dsvOffset, dsvSize);
	CreateDescriptors();
}

std::tuple<XMMATRIX, XMMATRIX, XMVECTOR, float, float> Effect::Shadow::RegisterLightVPXM() const
{
	XMVECTOR lightPos = -2.0f * Scene::sceneBounds.Radius * m_mainLight->GetLightDir();
	XMVECTOR target = XMLoadFloat3(&Scene::sceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, target, lightUp);

	// 将包围球变换到光源空间,计算出orthographic projection
	XMFLOAT3 sceneCenter;
	XMStoreFloat3(&sceneCenter, XMVector3TransformCoord(target, lightView)); // 矩阵向量相乘，变换到view空间
	float left = sceneCenter.x - Scene::sceneBounds.Radius;
	float right = sceneCenter.x + Scene::sceneBounds.Radius;
	float bottom = sceneCenter.y - Scene::sceneBounds.Radius;
	float top = sceneCenter.y + Scene::sceneBounds.Radius;
	float forward = sceneCenter.z - Scene::sceneBounds.Radius;
	float back = sceneCenter.z + Scene::sceneBounds.Radius;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(left, right, bottom, top, back, forward);
	return {lightView, lightProj, lightPos, back, forward};
}
