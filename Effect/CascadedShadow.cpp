#include "CascadedShadow.h"
#include "Texture.h"
#include "RtvDsvMgr.h"
#include "Scene.h"
#include <DirectXCollision.h>

using namespace Effect;
using namespace Models;

CascadedShadow::CascadedShadow(ID3D12Device* _device, UINT _width)
: RenderToTexture(_device, _width, _width, DXGI_FORMAT_R24G8_TYPELESS), cascadedUploader(std::make_unique<UploaderBuffer<CascadedShadowPass>>(_device, 1, true))
{
	m_dsvOffset = RtvDsvMgr::instance().RegisterDSV(cascadeLevels);
	m_passOffset = PassConstant::RegisterPassCount(cascadeLevels);
	CreateResources();
}

void CascadedShadow::CreateDescriptors(D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvStart, D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvStart,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDsvStart, UINT srvSize, UINT dsvSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSrvHandler(cpuSrvStart, static_cast<INT>(m_srvOffset), srvSize);
	m_cpuSRV = cpuSrvHandler;
	m_shadowCpuSRV[0] = cpuSrvHandler.Offset(1, srvSize);
	m_shadowCpuSRV[1] = cpuSrvHandler.Offset(1, srvSize);
	m_shadowCpuSRV[2] = cpuSrvHandler.Offset(1, srvSize);
	m_shadowCpuSRV[3] = cpuSrvHandler.Offset(1, srvSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrvHandler(gpuSrvStart, static_cast<INT>(m_srvOffset), srvSize);
	m_gpuSRV = gpuSrvHandler;
	m_shadowGpuSRV[0] = gpuSrvHandler.Offset(1, srvSize);
	m_shadowGpuSRV[1] = gpuSrvHandler.Offset(1, srvSize);
	m_shadowGpuSRV[2] = gpuSrvHandler.Offset(1, srvSize);
	m_shadowGpuSRV[3] = gpuSrvHandler.Offset(1, srvSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDsvHandler(cpuDsvStart, static_cast<INT>(m_dsvOffset), dsvSize);
	m_cpuDSV[0] = cpuDsvHandler;
	m_cpuDSV[1] = cpuDsvHandler.Offset(1, dsvSize);
	m_cpuDSV[2] = cpuDsvHandler.Offset(1, dsvSize);
	m_cpuDSV[3] = cpuDsvHandler.Offset(1, dsvSize);
	m_cpuDSV[4] = cpuDsvHandler.Offset(1, dsvSize);

	CreateDescriptors();
}

void CascadedShadow::InitShader(const wstring& csmName)
{
	m_shader = std::make_unique<Shader>(default_shader, csmName, initializer_list<D3D12_INPUT_ELEMENT_DESC>({
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	}));
}

void CascadedShadow::InitTexture(string_view csmName)
{
	auto InitCascaded = [](string_view csmName, UINT idx)
	{
		string cascadedName(csmName);
		cascadedName.append(to_string(idx));
		return TextureMgr::instance().RegisterRenderToTexture(cascadedName);
	};
	m_srvOffset = InitCascaded(csmName, 0);
	for (UINT i = 1; i < cascadeLevels; ++i)
	{
		InitCascaded(csmName, i);
	}
}

void CascadedShadow::InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowDesc = templateDesc;
	// 避免自遮挡的阴影偏移依赖于实际场景
	shadowDesc.RasterizerState.DepthBias = -10000;
	shadowDesc.RasterizerState.DepthBiasClamp = 0.0f;
	shadowDesc.RasterizerState.SlopeScaledDepthBias = -1.0f;
	shadowDesc.DepthStencilState.DepthEnable = true;
	shadowDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
	shadowDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	shadowDesc.VS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()),
	m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	shadowDesc.PS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	// shadow map不需要render target
	shadowDesc.NumRenderTargets = 0;
	shadowDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&shadowDesc, IID_PPV_ARGS(&m_pso)));
}

void CascadedShadow::Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc)
{
	float frustumStart = 0.0f, frustumEnd = 0.0f;
	const float cameraNearFarRange = abs(m_camera->m_farPlane - m_camera->m_nearPlane);
	auto [lightView, lightPos] = std::move(RegisterLightViewXM());
	m_shadowView = lightView;

	XMMATRIX camView = std::move(m_camera->GetCurrViewXM());
	XMMATRIX camProj = std::move(m_camera->GetCurrProjXM());
	XMMATRIX invCamView = XMMatrixInverse(nullptr, camView);
	// 划分子视锥体
	for (UINT idx = 0; idx < cascadeLevels; ++idx)
	{
		if (idx > 0)
		{
			frustumStart = cascadedPercent[idx - 1];
		}
		// 计算出视锥体Z的区间
		frustumEnd = cascadedPercent[idx];
		frustumStart *= cameraNearFarRange;
		frustumEnd *= cameraNearFarRange;
		// 为每个子视锥体创建光照空间下的正交投影, 通常一个正交投影包括了TopLeftX, TopLeftY,Width, Height，Near，Far
		XMFLOAT3 camFrustumPoints[8];
		BoundingFrustum camFrustum(camProj);
		camFrustum.Near = frustumStart;
		camFrustum.Far = frustumEnd;
		// 将子视锥体变换到世界空间在变换到光照空间
		camFrustum.Transform(camFrustum, XMMatrixMultiply(invCamView, lightView));
		camFrustum.GetCorners(camFrustumPoints);
		// 计算视锥体在光照空间下的AABB和VMax、VMin
		BoundingBox shadowAABB;
		BoundingBox::CreateFromPoints(shadowAABB, 8, camFrustumPoints, sizeof(XMFLOAT3));
		XMVECTOR lightAABBMin = XMLoadFloat3(&shadowAABB.Center) - XMLoadFloat3(&shadowAABB.Extents);
		XMVECTOR lightAABBMax = XMLoadFloat3(&shadowAABB.Center) + XMLoadFloat3(&shadowAABB.Extents);

		/*
		 * 避免级联时远近平面距离太小导致过短，产生AABB无法完成包围视锥体的问题
		 * near			    far
		 * 0____1		4________5
		 * |	|		|		 |	
		 * |	|		|        |
		 * |____|		|________|			
		 * 3	2       7		 6
		*/
		XMVECTOR diagVec = XMLoadFloat3(&camFrustumPoints[7]) - XMLoadFloat3(&camFrustumPoints[1]);
		XMVECTOR diag2Vec = XMLoadFloat3(&camFrustumPoints[7]) - XMLoadFloat3(&camFrustumPoints[5]);
		XMVECTOR length = XMVectorMax(XMVector3Length(diag2Vec), XMVector3Length(diagVec));
		XMVECTOR AABBOffset = (length - (lightAABBMax - lightAABBMin)) * g_XMOneHalf.v;
		static constexpr XMVECTORF32 onlyXY = { {1.0f, 1.0f, 0.0f, 0.0f} };
		lightAABBMax += AABBOffset * onlyXY.v;
		lightAABBMin -= AABBOffset * onlyXY.v;

		/*
		 * 需要消除由于光线变换或者视角变化导致阴影的闪烁。
		 * 原因在于两个时刻采样的深度位置不同，导致采样的深度值不同。因此我们考虑计算出阴影投射图的每个texel在世界中的宽度和高度，
		 * 令阴影投射图的款宽高分别是W、H的整数倍并且只能以W或H的整数倍进行移动
		 */ 
		float invSize = 1.0f / static_cast<float>(m_width);
		XMVECTOR worldUnitTexelSize = lightAABBMax - lightAABBMin;
		worldUnitTexelSize *= invSize;
		lightAABBMax /= worldUnitTexelSize;
		lightAABBMax = XMVectorFloor(lightAABBMax);
		lightAABBMax *= worldUnitTexelSize;
		lightAABBMin /= worldUnitTexelSize;
		lightAABBMin = XMVectorFloor(lightAABBMin);
		lightAABBMin *= worldUnitTexelSize;

		XMVECTOR sceneAABBPointLight[8]{};
		{
			XMFLOAT3 corners[8]{};
			camFrustum.GetCorners(corners);
			for (UINT i = 0; i < 8; ++i)
			{
				XMVECTOR v = XMLoadFloat3(&corners[i]);
				sceneAABBPointLight[i] = XMVector3Transform(v, lightView);
			}
		}
		// 计算AABB包围盒的nearZ和farZ, 做法是将场景的AABB的交点变换到光照空间，然后对这些点计算出min、max
		XMVECTOR lightMinVec = g_XMFltMax;
		XMVECTOR lightMaxVec = g_XMFltMin;
		for (UINT i = 0; i < 8; ++i)
		{
			lightMinVec = XMVectorMin(sceneAABBPointLight[i], lightMinVec);
			lightMaxVec = XMVectorMax(sceneAABBPointLight[i], lightMaxVec);
		}
		float farZ = XMVectorGetZ(lightMinVec);
		float nearZ = XMVectorGetZ(lightMaxVec);

		m_shadowProj[idx] = XMMatrixOrthographicOffCenterLH(
			XMVectorGetX(lightAABBMin), XMVectorGetX(lightAABBMax),
			XMVectorGetY(lightAABBMin), XMVectorGetY(lightAABBMax),
			nearZ, farZ);
		m_depthFloatFrustum[idx] = frustumEnd;
		XMMATRIX lightVP = XMMatrixMultiply(lightView, m_shadowProj[idx]);

		// Update Shadow Map PassConstant
		PassConstant shadowPass{};
		XMStoreFloat4x4(&shadowPass.view_gpu, XMMatrixTranspose(lightView));
		XMStoreFloat4x4(&shadowPass.proj_gpu, XMMatrixTranspose(m_shadowProj[idx]));
		XMStoreFloat4x4(&shadowPass.vp_gpu, XMMatrixTranspose(lightVP));
		shadowPass.nearZ_gpu = nearZ;
		shadowPass.farZ_gpu = farZ;
		shadowPass.renderTargetSize_gpu = std::move(XMFLOAT2(
			static_cast<float>(m_width), static_cast<float>(m_height)));
		shadowPass.invRenderTargetSize_gpu = std::move(XMFLOAT2(
			1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height)));
		XMStoreFloat3(&shadowPass.cameraPos_gpu, lightPos);
		updateFunc(m_passOffset + idx, shadowPass);
	}
	// Update Cascaded Shadow Map Uploader Buffer
	SyncWithShadowPass();
}

void CascadedShadow::Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const
{
	cmdList->RSSetViewports(1, &m_viewport);
	cmdList->RSSetScissorRects(1, &m_scissorRect);
	for (UINT idx = 0; idx < cascadeLevels; ++idx)
	{
		ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE>(cmdList, GetCascadedRes(idx));
		cmdList->ClearDepthStencilView(m_cpuDSV[idx], D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
		cmdList->OMSetRenderTargets(0, nullptr, false, &m_cpuDSV[idx]);
		cmdList->SetPipelineState(m_pso.Get());
		drawFunc(m_passOffset + idx);
		ChangeState<D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, GetCascadedRes(idx));
	}
}

void CascadedShadow::SetNecessaryParameters(float _offset, float _range, const shared_ptr<Camera>& _viewCam, const Light* _mainLight, int kernelSize)
{
	m_shadowOffset = _offset;
	m_cascadedBlend = std::clamp(_range, 0.0f, 1.0f);
	m_camera = _viewCam;
	m_mainLight = _mainLight;
	m_kernelSize = kernelSize;
}

void CascadedShadow::CopyCascadedShadowPass(ID3D12GraphicsCommandList* cmdList) const
{
	const auto address = cascadedUploader->GetResource()->GetGPUVirtualAddress();
	cmdList->SetGraphicsRootConstantBufferView(11, address);
}

XMFLOAT4X4 CascadedShadow::GetShadowView() const
{
	XMFLOAT4X4 ans;
	XMStoreFloat4x4(&ans, m_shadowView);
	return ans;
}

const XMMATRIX& CascadedShadow::GetShadowViewXM() const
{
	return m_shadowView;
}

ID3D12Resource* CascadedShadow::GetCascadedRes(UINT idx) const
{
	assert(idx < cascadeLevels);
	if (idx == 0)
		return m_resource.Get();
	return m_shadowRes[idx - 1].Get();
}

UINT CascadedShadow::GetCascadedSrvOffset() const
{
	return m_srvOffset;
}

void CascadedShadow::SyncWithShadowPass()
{
	static XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	XMFLOAT4 scales[cascadeLevels]{};
	XMFLOAT4 offsets[cascadeLevels]{};
	for (UINT idx = 0; idx < cascadeLevels; ++idx)
	{
		const XMMATRIX shadowTex = XMMatrixMultiply(m_shadowProj[idx], T);
		scales[idx].x = XMVectorGetX(shadowTex.r[0]);
		scales[idx].y = XMVectorGetY(shadowTex.r[1]);
		scales[idx].z = XMVectorGetZ(shadowTex.r[2]);
		scales[idx].w = 1.0f;
		XMStoreFloat3((XMFLOAT3*)(offsets + idx), shadowTex.r[3]);
	}
	float depthArray[cascadeLevels][4] = {
		{m_depthFloatFrustum[0]}, {m_depthFloatFrustum[1]}, {m_depthFloatFrustum[2]},
		{m_depthFloatFrustum[3]}, {m_depthFloatFrustum[4]} };

	CascadedShadowPass shadowPass{};
	memcpy(&shadowPass.cascadedScale_gpu, &scales, sizeof(XMFLOAT4) * cascadeLevels);
	memcpy(&shadowPass.cascadedOffset_gpu, &offsets, sizeof(XMFLOAT4) * cascadeLevels);
	memcpy(&shadowPass.cascadedDepthFloat_gpu, &depthArray, sizeof(XMFLOAT4) * cascadeLevels);
	shadowPass.cascadedShadowPercent_gpu[0] = XMFLOAT4(cascadedPercent[0], cascadedPercent[1], cascadedPercent[2], cascadedPercent[3]);
	shadowPass.cascadedShadowPercent_gpu[1] = XMFLOAT4(cascadedPercent[4], 0.0f, 0.0f, 0.0f);
	shadowPass.cascadedShadowOffset = m_shadowOffset;
	shadowPass.cascadedBlend_gpu = m_cascadedBlend;
	shadowPass.pcfStart_gpu = -m_kernelSize;
	shadowPass.pcfEnd_gpu = m_kernelSize;
	cascadedUploader->Copy(0, shadowPass);
}

std::tuple<XMMATRIX, XMVECTOR> CascadedShadow::RegisterLightViewXM() const
{
	XMVECTOR lightPos = -2.0f * Scene::sceneBounds.Radius * m_mainLight->GetLightDir();
	XMVECTOR target = XMLoadFloat3(&Scene::sceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, target, lightUp);

	return { lightView, lightPos};
}

void CascadedShadow::CreateResources()
{
	SetViewPortAndScissor();

	D3D12_RESOURCE_DESC shadowDesc{};
	ZeroMemory(&shadowDesc, sizeof(D3D12_RESOURCE_DESC));
	shadowDesc.Alignment = 0;
	shadowDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	shadowDesc.Width = m_width;
	shadowDesc.Height = m_height;
	shadowDesc.Format = m_format;
	shadowDesc.DepthOrArraySize = 1;
	shadowDesc.MipLevels = 1;
	shadowDesc.SampleDesc.Count = 1;
	shadowDesc.SampleDesc.Quality = 0;
	shadowDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	shadowDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 填充纹理
	D3D12_CLEAR_VALUE shadowClear;
	shadowClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	shadowClear.DepthStencil.Depth = 0.0f;
	shadowClear.DepthStencil.Stencil = 0;

	const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &shadowDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &shadowClear, IID_PPV_ARGS(m_resource.GetAddressOf())));
	for (UINT i = 0; i < 4; ++i)
	{
		ThrowIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &shadowDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &shadowClear, IID_PPV_ARGS(m_shadowRes[i].GetAddressOf())));
	}
}

void CascadedShadow::CreateDescriptors()
{
	// 创建SRV让shader能够采样shadowMap
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	m_device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_cpuSRV);
	for (UINT i = 0; i < 4; ++i)
	{
		m_device->CreateShaderResourceView(m_shadowRes[i].Get(), &srvDesc, m_shadowCpuSRV[i]);
	}

	// 创建DSV让shader能够渲染到shadow Map上
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	m_device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, m_cpuDSV[0]);
	for (UINT i = 0; i < 4; ++i)
	{
		m_device->CreateDepthStencilView(m_shadowRes[i].Get(), &dsvDesc, m_cpuDSV[i + 1]);
	}
}
