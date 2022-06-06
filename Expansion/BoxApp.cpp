#include "BoxApp.h"
#include <DirectXColors.h>
#include <vector>
#include <DirectXMath.h>
#include <algorithm>
#include <memory>
#include <xstring>
#include <dxgi1_6.h>
#include <d3dcommon.h>
#include <iostream>
#include "BaseGeometry.h"
#include "Texture.h"
#include "ObjLoader.h"
#include "PostProcessMgr.hpp"
#include "Scene.h"
#if defined(DEBUG) || defined(_DEBUG)
#include "DebugMgr.hpp"
#endif

using namespace DirectX;

BoxApp::BoxApp(HINSTANCE instance, bool startMsaa, Camera::CameraType type)
: D3DApp_Template(instance, startMsaa, type), m_material(std::make_shared<Material>()), m_skybox(std::make_unique<Effect::CubeMap>())
{
	m_passOffset = PassConstant::RegisterPassCount(1);
	m_pixelLights.reserve(maxLights);
	m_computeLights.reserve(pointLightNum);
	m_pointLightPos.reserve(pointLightNum);
}

bool BoxApp::Init()
{
	if(!D3DApp_Template::Init(*this))
		return false;
	m_commandList->Reset(m_commandAllocator.Get(), nullptr);

	CreateLights();
	CreateTextures();
	CreateRootSignature();
	CreateDescriptorHeaps();
	CreateShadersAndInput();
	CreateGeometry();
	CreateMaterials();
	CreateRenderItems();
	CreateFrameResources();
	CreatePSO();

	// ִ��������г�ʼ������
	m_commandList->Close();
	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	FlushCommandQueue();

	UpdateLUT(m_timer);
	DrawLUT();

	return true;
}

BoxApp::~BoxApp()
{
	if (m_d3dDevice != nullptr)
		FlushCommandQueue();
}

void BoxApp::Resize()
{
	D3DApp_Template::Resize(*this);
	gBuffer->Resize(m_clientWidth, m_clientHeight);
	m_renderer->OnResize(m_clientWidth, m_clientHeight);
	m_blur->OnResize(m_clientWidth, m_clientHeight);
	m_toneMap->OnResize(m_clientWidth, m_clientHeight);
	m_TemporalAA->OnResize(m_clientWidth, m_clientHeight);
}

void BoxApp::Update(const GameTimer& timer)
{
	OnKeyboardInput(timer);

	// ѭ����ȡ��ǰ֡��Դ������
	m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % frameResourcesCount;
	m_currFrameResource = m_frameCBuffer[m_currFrameResourceIndex].get();

	// GPU����δִ���굱ǰ֡��Դ����������,����CPUִ�й���, CPU��Ҫ����ȴ�״ֱ̬��GPU����������Χ����
	if (m_currFrameResource->m_fence != 0 && m_fence->GetCompletedValue() < m_currFrameResource->m_fence)
	{
		HANDLE eventHandler = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		m_fence->SetEventOnCompletion(m_currFrameResource->m_fence, eventHandler);
		WaitForSingleObject(eventHandler, INFINITE);
		CloseHandle(eventHandler);
	}

	{
		XMMATRIX rotate = XMMatrixRotationY(static_cast<float>(0.1 * timer.DeltaTime()));
		XMVECTOR lightDir = XMVector3TransformNormal(m_pixelLights[0]->GetLightDir(), rotate);
		XMStoreFloat3(&const_cast<XMFLOAT3&>(m_pixelLights[0]->GetData().direction), lightDir);
		UpdateLightPos(timer);
	}

	UpdateObjectInstance(timer);
	UpdatePassConstant(timer);
	UpdateMaterialConstant(timer);
	UpdatePostProcess(timer);
	UpdateOffScreen(timer);
}

void BoxApp::DrawScene(const GameTimer& timer)
{
	auto alloc = m_currFrameResource->m_commandAllocator;
	ThrowIfFailed(alloc->Reset());
	ThrowIfFailed(m_commandList->Reset(alloc.Get(), gBuffer->m_pso.Get()));

	// ����ǩ����CBV/SRV�����õ����������
	ID3D12DescriptorHeap* descriptorSRVHeaps[] = { TextureMgr::instance().GetSRVDescriptorHeap() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorSRVHeaps), descriptorSRVHeaps);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// matBuffer����
	auto matBuffer = m_currFrameResource->m_materialCBuffer->GetResource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	// textureBuffer����
	m_commandList->SetGraphicsRootDescriptorTable(6, TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	m_shadow->CopyCascadedShadowPass(m_commandList.Get());

	/*
	 * ʵʱ������Ⱦ����
	 */
	m_shadow->Draw(m_commandList.Get(), [&](UINT offset)
	{
		constexpr UINT passCBSize = D3DUtil::AlignsConstantBuffer(sizeof(PassConstant));
		auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
		auto address = passCB->GetGPUVirtualAddress() + offset * passCBSize;
		m_commandList->SetGraphicsRootConstantBufferView(0, address);
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
	});
	m_TemporalAA->FirstDraw(m_commandList.Get(), GetDepthStencilView(), m_rootSignature.Get(), [&]()
	{
		m_commandList->RSSetViewports(1, &m_camera->GetViewPort());
		m_commandList->RSSetScissorRects(1, &m_scissorRect);
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
		m_commandList->SetPipelineState(m_skybox->GetPSO());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::skybox)]);
	});

	m_commandList->RSSetViewports(1, &m_camera->GetViewPort());
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	ChangeState<D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET>(m_commandList.Get(), GetCurrentBackBuffer());
	gBuffer->RefreshGBuffer(m_commandList.Get());
	m_commandList->ClearRenderTargetView(GetCurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	m_commandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,0.0f, 0, 0, nullptr);

	// ָ����Ⱦ������
	{
		const auto& depthStencilView = GetDepthStencilView();
		m_commandList->OMSetRenderTargets(3, &gBuffer->gBufferRTV[0], true, &depthStencilView);
	}

	// ͨ�����εķ�ʽ��CBV��ĳ�����������໥��
	m_commandList->SetPipelineState(gBuffer->m_pso.Get());
	auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
	auto address = passCB->GetGPUVirtualAddress();
	m_commandList->SetGraphicsRootConstantBufferView(m_passOffset, address);
	DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);

	for (int i = 0; i < 3; ++i)
	{
		ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(m_commandList.Get(), gBuffer->gBufferRes[i].Get());
	}
	// ��Post Process�д���GBuffer��PassConstant
	// ��GPU�д���GBuffer����
	auto gBufferSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	gBufferSRVHandler.Offset(gBuffer->albedoIdx, m_cbvUavDescriptorSize);
	PostProcessMgr::instance().UpdateResources<PostProcessMgr::Compute>(m_commandList.Get(), m_currFrameResource->m_postProcessCBuffer->GetResource()->GetGPUVirtualAddress(), gBufferSRVHandler);
	m_commandList->SetGraphicsRootDescriptorTable(3, gBufferSRVHandler);

	// SSAO����
	m_ssao->Draw(m_commandList.Get(), [](UINT) {});
	auto ssaoSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	ssaoSRVHandler.Offset(m_ssao->GetSrvIdx("SSAO").value_or(0), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(4, ssaoSRVHandler);

	// ��GPU�д���shadow����
	auto shadowSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	shadowSRVHandler.Offset(m_shadow->GetCascadedSrvOffset(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(5, shadowSRVHandler);
	auto skyboxSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	skyboxSRVHandler.Offset(m_skybox->GetStaticID(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(7, skyboxSRVHandler);

	m_renderer->Draw(m_commandList.Get(), [&](UINT){
		m_commandList->SetComputeRootConstantBufferView(0, m_currFrameResource->m_postProcessCBuffer->GetResource()->GetGPUVirtualAddress());
		m_commandList->SetComputeRootDescriptorTable(1, gBufferSRVHandler);
	});
	m_commandList->SetPipelineState(m_skybox->GetPSO());
	DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::skybox)]);

	/*
	 * Post Process Part
	 */
	PostProcessMgr::instance().UpdateResources<PostProcessMgr::Graphics>(m_commandList.Get(), m_currFrameResource->m_postProcessCBuffer->GetResource()->GetGPUVirtualAddress(), gBufferSRVHandler);
	for (int i = 0; i < 3; ++i)
	{
		ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(m_commandList.Get(), gBuffer->gBufferRes[i].Get());
	}
	// ������Դ����;ָʾ��״̬��ת�䣬����ȾĿ��״̬ת��Ϊ����״̬
	DrawPostProcess(m_commandList.Get());

	// �������ļ�¼
	ThrowIfFailed(m_commandList->Close());
	// ����������������ִ�е������б�
	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	// ����ǰ��̨������
	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currBackBuffer = (m_currBackBuffer + 1) % m_swapBufferCount;
	// ͨ��֡��Դ�滻FlushCommandQueue,ʵ�����޷���ȫ����ȴ�״���ķ���������������б��ַǿգ�ʹ��GPU��������ִ��
	m_currFrameResource->m_fence = ++m_currFence;
	m_commandQueue->Signal(m_fence.Get(), m_currFence);
}

void BoxApp::OnMouseDown(WPARAM btn_state, int x, int y)
{
	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
	SetCapture(mainWindow);
}

void BoxApp::OnMouseMove(WPARAM btn_state, int x, int y)
{
	if ((btn_state & MK_LBUTTON) != 0)
	{
		// ���������ƶ����������ת�Ƕȣ���ÿ�����ض����˽Ƕȵ�0.25��s��ת
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

		m_camera->Pitch(dy);
		m_camera->Rotate(dx);
	}

	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
}

void BoxApp::OnMouseUp(WPARAM btn_state, int x, int y)
{
	ReleaseCapture();
}

void BoxApp::OnKeyboardInput(const GameTimer& timer)
{
	const float delta = timer.DeltaTime();
	if (GetAsyncKeyState('W') & 0x8000)
		m_camera->MoveForward(20.0f * delta);
	if (GetAsyncKeyState('S') & 0x8000)
		m_camera->MoveForward(-20.0f * delta);
	if (GetAsyncKeyState('A') & 0x8000)
		m_camera->Strafe(-20.0f * delta);
	if (GetAsyncKeyState('D') & 0x8000)
		m_camera->Strafe(20.0f * delta);

	m_camera->SetJitter(m_TemporalAA->GetJitter());
	m_camera->Update();
}

void BoxApp::RegisterRTVAndDSV() {
	RtvDsvMgr::instance().RegisterRTV(m_swapBufferCount );
	RtvDsvMgr::instance().RegisterDSV(1);
}

void BoxApp::CreateOffScreenRendering() {
	Models::ObjLoader::instance().Init(m_d3dDevice.Get(), m_commandList.Get());
	PostProcessMgr::instance().Init(m_d3dDevice.Get());
	m_dynamicCube = std::make_unique<Effect::DynamicCubeMap>(m_d3dDevice.Get(), 1024U, 1024U, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_dynamicCube->InitCamera(0.0f, 2.0f, 0.0f);
	gBuffer = std::make_unique<Renderer::GBuffer>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R16G16B16A16_SNORM);
	m_renderer = std::make_unique<Renderer::TileBasedDefer>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, m_backBufferFormat);
	m_shadow = std::make_unique<Effect::CascadedShadow>(m_d3dDevice.Get(), 1024U);
	m_blur = std::make_unique<Effect::GaussianBlur>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 2U);
	m_toneMap = std::make_unique<Effect::ToneMap>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, m_backBufferFormat);
	m_ssao = std::make_unique<Effect::SSAO>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, DXGI_FORMAT_R8_UNORM);
	m_TemporalAA = std::make_unique<Effect::TemporalAA>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
#if defined(DEBUG) || defined(_DEBUG)
	Debug::DebugMgr::instance().InitRequiredParameters(m_d3dDevice.Get());
#endif
}

void BoxApp::UpdateLUT(const GameTimer& timer)
{
	Update(timer);
	m_dynamicCube->Update(timer, [&](UINT i, auto& constant) {
		constant.lights[0].direction = m_pixelLights[0]->GetData().direction;
		constant.lights[0].strength = m_pixelLights[0]->GetData().strength;
		constant.lights[1].direction = m_pixelLights[1]->GetData().direction;
		constant.lights[1].strength = m_pixelLights[1]->GetData().strength;
		constant.lights[2].direction = m_pixelLights[2]->GetData().direction;
		constant.lights[2].strength = m_pixelLights[2]->GetData().strength;

		auto currPass = m_currFrameResource->m_passCBuffer.get();
		currPass->Copy(i, constant);
	});
}

void BoxApp::DrawLUT()
{
	auto LUTParams = [&]() ->ComPtr<ID3D12PipelineState>
	{
		std::unique_ptr<Shader> temp = std::make_unique<Shader>(default_shader, L"Shaders\\Forward", initializer_list<D3D12_INPUT_ELEMENT_DESC>({
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}}));
		ComPtr<ID3D12PipelineState> luts;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC lutDesc{};
		ZeroMemory(&lutDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		lutDesc.InputLayout = { temp->GetInputLayouts(), temp->GetInputLayoutSize() };
		lutDesc.pRootSignature = m_rootSignature.Get();
		lutDesc.VS = {
			static_cast<BYTE*>(temp->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()),
			temp->GetShaderByType(ShaderPos::vertex)->GetBufferSize()
		};
		lutDesc.PS = {
			static_cast<BYTE*>(temp->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),
			temp->GetShaderByType(ShaderPos::fragment)->GetBufferSize()
		};
		lutDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		lutDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		lutDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		lutDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		lutDesc.SampleMask = UINT_MAX;
		lutDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		lutDesc.NumRenderTargets = 2u;
		lutDesc.RTVFormats[0] = m_backBufferFormat;
		lutDesc.RTVFormats[1] = m_backBufferFormat;
		lutDesc.SampleDesc.Count = m_msaaState ? 4u : 1u;
		lutDesc.SampleDesc.Quality = m_msaaState ? (m_msaaQuality - 1u) : 0u;
		lutDesc.DSVFormat = m_depthStencilFormat;
		ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&lutDesc, IID_PPV_ARGS(&luts)));
		return luts;
	}();
	auto alloc = m_currFrameResource->m_commandAllocator;
	ThrowIfFailed(alloc->Reset());
	ThrowIfFailed(m_commandList->Reset(alloc.Get(), LUTParams.Get()));

	// ����ǩ����CBV/SRV�����õ����������
	ID3D12DescriptorHeap* descriptorSRVHeaps[] = { TextureMgr::instance().GetSRVDescriptorHeap() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorSRVHeaps), descriptorSRVHeaps);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// matBuffer����
	auto matBuffer = m_currFrameResource->m_materialCBuffer->GetResource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	// textureBuffer����
	m_commandList->SetGraphicsRootDescriptorTable(6, TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	// ��GPU�д���skybox����
	auto skyboxSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	skyboxSRVHandler.Offset(m_skybox->GetStaticID(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(7, skyboxSRVHandler);

	// LUT Shadow
	m_shadow->Draw(m_commandList.Get(), [&](UINT offset) {
		constexpr UINT passCBSize = D3DUtil::AlignsConstantBuffer(sizeof(PassConstant));
		auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
		auto address = passCB->GetGPUVirtualAddress() + offset * passCBSize;
		m_commandList->SetGraphicsRootConstantBufferView(0, address);
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
	});

	// ��GPU�д���shadow����
	auto shadowSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	shadowSRVHandler.Offset(m_shadow->GetCascadedSrvOffset(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(5, shadowSRVHandler);
	// LUT drawing
	m_dynamicCube->Draw(m_commandList.Get(), [&](UINT offset) {
		constexpr auto passCBSize = D3DUtil::AlignsConstantBuffer(sizeof(PassConstant));
		auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
		auto address = passCB->GetGPUVirtualAddress() + (offset + 1) * passCBSize;
		m_commandList->SetGraphicsRootConstantBufferView(0, address);
		m_commandList->SetPipelineState(LUTParams.Get());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
		m_commandList->SetPipelineState(m_skybox->GetPSO());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::skybox)]);
	});

	m_TemporalAA->FirstDraw(m_commandList.Get(), GetDepthStencilView(), [&]()
	{
		m_commandList->SetPipelineState(LUTParams.Get());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
		m_commandList->SetPipelineState(m_skybox->GetPSO());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::skybox)]);
	});

	// �������ļ�¼
	ThrowIfFailed(m_commandList->Close());
	// ����������������ִ�е������б�
	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	// ����ǰ��̨������
	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currBackBuffer = (m_currBackBuffer + 1) % m_swapBufferCount;
	FlushCommandQueue();
}

void BoxApp::CreateLights()
{
	LightInPixel dirLight0;
	dirLight0.direction = { 0.57735f, -0.57735f, 0.57735f };
	dirLight0.strength = { 1.0f, 1.0f, 1.0f };
	m_pixelLights.emplace_back(std::make_shared<Light<Pixel>>(std::move(dirLight0)));
	LightInPixel dirLight1;
	dirLight1.direction = { -0.57735f, -0.57735f, 0.57735f };
	dirLight1.strength = { 0.8f, 0.8f, 0.8f };
	m_pixelLights.emplace_back(std::make_shared<Light<Pixel>>(std::move(dirLight1)));
	LightInPixel dirLight2;
	dirLight2.direction = { 0.0f, 0.707f, 0.707f };
	dirLight2.strength = { 0.8f, 0.8f, 0.8f };
	m_pixelLights.emplace_back(std::make_shared<Light<Pixel>>(std::move(dirLight2)));
	m_shadow->SetNecessaryParameters(0.001f, 0.2f, m_camera, m_pixelLights[0].get(), 2);

	std::mt19937 randSeed(1337);
	constexpr float maxRadius = 100.0f;
	constexpr float attenuation = 0.8f;
	std::uniform_real<float> radiusNormDist(0.0f, 1.0f);
	std::uniform_real<float> angleDist(0.0f, 2.0f * XM_PI);
	std::uniform_real<float> heightDist(0.0f, 75.0f);
	std::uniform_real<float> moveSpeedDist(2.0f, 20.0f);
	std::uniform_int<int>	 moveDirection(0, 1);
	std::uniform_real<float> hueDist(0.0f, 1.0f);
	std::uniform_real<float> intensityDist(0.2f, 0.8f);
	std::uniform_real<float> attenuationDist(2.0f, 20.0f);
	for (UINT i = 0; i < pointLightNum; ++i)
	{
		LightMoveParams data;
		data.radius = std::sqrt(radiusNormDist(randSeed)) * maxRadius;
		data.angle = angleDist(randSeed);
		data.height = heightDist(randSeed);
		data.moveSpeed = (moveDirection(randSeed) * 2 - 1) * moveSpeedDist(randSeed) / data.radius;
		m_pointLightPos.emplace_back(std::make_unique<LightMoveParams>(data));

		LightInCompute params;
		params.position = { data.radius * std::cos(data.angle), data.height, data.radius * std::sin(data.angle) };
		params.fallOffEnd = attenuationDist(randSeed);
		params.fallOffStart = attenuation * params.fallOffEnd;
		params.strength = MathHelper::MathHelper::HueToRGB(hueDist(randSeed));
		XMStoreFloat3(&params.strength, XMLoadFloat3(&params.strength) * intensityDist(randSeed));
		m_computeLights.emplace_back(std::make_shared<Light<Compute>>(std::move(params)));
	}
}

void BoxApp::CreateDescriptorHeaps()
{
	TextureMgr::instance().GenerateSRVHeap();

	auto cpuSrvStart = TextureMgr::instance().GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	auto gpuSrvStart = TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	auto cpuRtvStart = RtvDsvMgr::instance().GetRenderTargetView();
	auto cpuDsvStart = RtvDsvMgr::instance().GetDepthStencilView();
	gBuffer->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuRtvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize);
	m_dynamicCube->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuDsvStart, cpuRtvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize, m_dsvDescriptorSize);
	m_shadow->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuDsvStart, m_cbvUavDescriptorSize, m_dsvDescriptorSize);
	m_blur->CreateDescriptors(cpuSrvStart, gpuSrvStart, m_cbvUavDescriptorSize);
	m_toneMap->CreateDescriptors(cpuSrvStart, gpuSrvStart, m_cbvUavDescriptorSize);
	m_renderer->CreateDescriptors(cpuSrvStart, cpuRtvStart, GetDepthStencilView(), gpuSrvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize, m_dsvDescriptorSize);
	m_ssao->CreateRandomTexture(m_commandList.Get());
	m_ssao->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuRtvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize);
	m_TemporalAA->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuRtvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize);
}

// ��ǩ����ִ�л�������֮ǰ��Ӧ�óɳ��򽫰󶨵���ˮ���ϵ���Դӳ�䵽��Ӧ������Ĵ�����
// ��ǩ����Ҫ������С����Ϊ��ǩ��Խ�����ʵ�εĿ���Ҳ��Խ��ͬʱΪ�˱���Ƶ���л���ǩ����Ҫ�����������ձ��Ƶ���ɸߵ�������
void BoxApp::CreateRootSignature()
{
	//CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	//// ����������������, �󶨵Ļ�׼��ɫ���Ĵ���λ��
	//cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	//CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	//cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	CD3DX12_DESCRIPTOR_RANGE skyboxSRV;
	skyboxSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	CD3DX12_DESCRIPTOR_RANGE randSRV;
	randSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	CD3DX12_DESCRIPTOR_RANGE ssaoSRV;
	ssaoSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	CD3DX12_DESCRIPTOR_RANGE texSRV;
	texSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 256, 4, 0);
	CD3DX12_DESCRIPTOR_RANGE gBufferSRV;
	gBufferSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 4, 1);
	CD3DX12_DESCRIPTOR_RANGE shadowSRV;
	shadowSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 4, 2);
	CD3DX12_ROOT_PARAMETER parameters[12]{};
	parameters[0].InitAsConstantBufferView(0); // ��Ⱦ���̵�CBV
	parameters[1].InitAsShaderResourceView(1, 0); // �����CBV
	parameters[2].InitAsShaderResourceView(0, 1); // ������ʵĽṹ��������
	parameters[3].InitAsDescriptorTable(1, &gBufferSRV, D3D12_SHADER_VISIBILITY_PIXEL); // gBuffer��λ��
	parameters[4].InitAsDescriptorTable(1, &ssaoSRV, D3D12_SHADER_VISIBILITY_PIXEL);
	parameters[5].InitAsDescriptorTable(1, &shadowSRV, D3D12_SHADER_VISIBILITY_PIXEL);// ��Ӱ��ͼ��λ��
	parameters[6].InitAsDescriptorTable(1, &texSRV, D3D12_SHADER_VISIBILITY_PIXEL); // ���������λ��
	parameters[7].InitAsDescriptorTable(1, &skyboxSRV, D3D12_SHADER_VISIBILITY_PIXEL); // ��պе�λ��
	parameters[8].InitAsDescriptorTable(1, &randSRV, D3D12_SHADER_VISIBILITY_PIXEL); // SSAO���ת������
	parameters[9].InitAsConstantBufferView(1); // SSAO ConstantBuffer����
	parameters[10].InitAsConstantBufferView(2); // Bilateral Blur ����
	parameters[11].InitAsConstantBufferView(3);

	// ��̬����������
	auto staticSampler = CreateStaticSampler2D();
	// ��ǩ���Ĳ������
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(12U, parameters, staticSampler.size(), staticSampler.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	// ����ֻ����һ��������������ɵ�����������ĸ�ǩ��
	ComPtr<ID3DBlob> serialRootSig{ nullptr }; // ID3DBlob��һ����ͨ���ڴ�飬���Է���һ��void*���ݻ򷵻ػ������Ĵ�С
	ComPtr<ID3DBlob> error{ nullptr };
	HRESULT res = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, serialRootSig.GetAddressOf(), error.GetAddressOf());
	if (error) {
		OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
	}
	ThrowIfFailed(res);
	m_d3dDevice->CreateRootSignature(0, serialRootSig->GetBufferPointer(), 
		serialRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));

	m_renderer->InitRootSignature();
}

void BoxApp::CreateShadersAndInput() {
	/// <summary>
	/// make_unique�Ǹ�����ģ�壬ֻ���Ƶ����ݸ������캯���Ĳ������Ͷ��������б��ǲ����Ƶ���
	/// </summary>
	gBuffer->InitShaders(std::forward_as_tuple(L"Shaders\\GBuffer", default_shader, initializer_list<D3D12_INPUT_ELEMENT_DESC>({
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}})));
	m_shadow->InitShader(L"Shaders\\Shadow");
	m_dynamicCube->InitShader(L"Shaders\\Skybox");
	m_blur->InitShader(L"Shaders\\Blur_Horizontal", L"Shaders\\Blur_Vertical");
	m_toneMap->InitShader(L"Shaders\\ToneMap_ACES");
	m_renderer->InitShaders(std::forward_as_tuple(L"Shaders\\Box", default_shader, initializer_list<D3D12_INPUT_ELEMENT_DESC>({ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} })), 
		std::forward_as_tuple(L"Shaders\\TileBased_Defer", compute_shader, initializer_list<D3D12_INPUT_ELEMENT_DESC>()));
	m_ssao->InitShader();
	m_TemporalAA->InitShader(L"Shaders\\TemporalAA_Aliasing", L"Shaders\\MotionVector");
}

void BoxApp::CreateGeometry()
{
	auto totalGeo = std::make_unique<Mesh>();
	BaseMeshData sphere = BaseGeometry::CreateSphere(0.5f, 20, 20);
	BaseMeshData quad = BaseGeometry::CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	///*
	// * �����еļ�����ϲ���һ�Ѵ�Ķ��㡢������������
	//*/
	UINT vboOffset = 0;
	UINT eboOffset = 0;
	// ����Ķ��submeshGeometry�����˶���/�����������ڲ�ͬ�����������������
	Submesh sphereSubmesh;
	Submesh quadSubmesh;
	sphereSubmesh.eboCount = static_cast<UINT>(sphere.EBOs.size());
	sphereSubmesh.eboStart = eboOffset;
	sphereSubmesh.vboStart = vboOffset;
	vboOffset += sphere.VBOs.size();
	eboOffset += sphere.EBOs.size();

	quadSubmesh.eboCount = static_cast<UINT>(quad.EBOs.size());
	quadSubmesh.eboStart = eboOffset;
	quadSubmesh.vboStart = vboOffset;
	vboOffset += quad.VBOs.size();
	eboOffset += quad.EBOs.size();

	vector<Vertex_GPU> vbo;
	vector<uint16_t> ebo;
	// ��ȡ���еĶ���Ԫ�ز���ʽһ�����㻺����
	for (auto i = 0; i < sphere.VBOs.size(); ++i)
		vbo.emplace_back(Vertex_GPU{ sphere.VBOs[i].pos, sphere.VBOs[i].normal, sphere.VBOs[i].tangent, sphere.VBOs[i].tex });
	ebo.insert(ebo.end(), std::begin(sphere.GetEBO_16()), std::end(sphere.GetEBO_16()));

	for (auto i = 0; i < quad.VBOs.size(); ++i)
		vbo.emplace_back(Vertex_GPU{ quad.VBOs[i].pos, quad.VBOs[i].normal, quad.VBOs[i].tangent, quad.VBOs[i].tex });
	ebo.insert(ebo.end(), std::begin(quad.GetEBO_16()), std::end(quad.GetEBO_16()));

	auto objModel = Models::ObjLoader::instance().GetObj("Sponza/pbr/sponza.obj").value();
	const UINT len = objModel->meshData.size();
	for (UINT i = 0; i < len; ++i)
	{
		string name("sponza" + to_string(i));
		Submesh temp;
		temp.vboStart = vboOffset;
		temp.eboStart = eboOffset;
		temp.eboCount = objModel->meshData[i].EBOs.size();

		vbo.reserve(vbo.size() + objModel->meshData[i].VBOs.size());
		for (auto& vbo_cpu : objModel->meshData[i].VBOs)
		{
			vbo.emplace_back(Vertex_GPU{ vbo_cpu.pos, vbo_cpu.normal, vbo_cpu.tangent, vbo_cpu.tex });
		}
		ebo.insert(ebo.end(), std::begin(objModel->meshData[i].GetEBO_16()), std::end(objModel->meshData[i].GetEBO_16()));

		vboOffset += objModel->meshData[i].VBOs.size();
		eboOffset += objModel->meshData[i].EBOs.size();
		totalGeo->drawArgs[name] = std::move(temp);
	}

	const UINT vboByteSize = static_cast<UINT>(vbo.size()) * static_cast<UINT>(sizeof(Vertex_GPU));
	const UINT eboByteSize = static_cast<UINT>(ebo.size()) * static_cast<UINT>(sizeof(uint16_t));
	totalGeo->name = "Total";
	D3DCreateBlob(vboByteSize, &totalGeo->vbo_cpu);
	CopyMemory(totalGeo->vbo_cpu->GetBufferPointer(), vbo.data(), vboByteSize);
	D3DCreateBlob(eboByteSize, &totalGeo->ebo_cpu);
	CopyMemory(totalGeo->ebo_cpu->GetBufferPointer(), ebo.data(), eboByteSize);

	totalGeo->vbo_gpu = D3DUtil::CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), vbo.data(), vboByteSize, totalGeo->vbo_uploader);
	totalGeo->ebo_gpu = D3DUtil::CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), ebo.data(), eboByteSize, totalGeo->ebo_uploader);
	totalGeo->vbo_stride = sizeof(Vertex_GPU);
	totalGeo->vbo_bufferByteSize = vboByteSize;
	totalGeo->ebo_format = DXGI_FORMAT_R16_UINT;
	totalGeo->ebo_bufferByteSize = eboByteSize;

	totalGeo->drawArgs["Sphere"] = std::move(sphereSubmesh);
	totalGeo->drawArgs["Debug"] = std::move(quadSubmesh);
	m_meshGeos[totalGeo->name] = std::move(totalGeo);
}

// ��ˮ��״̬�������벼�������ѡ�������ɫ����������ɫ���͹�դ��״̬��󶨵�ͼ����ˮ���� 
void BoxApp::CreatePSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC templateDesc;
	ZeroMemory(&templateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	templateDesc.pRootSignature = m_rootSignature.Get();
	templateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	templateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	templateDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	
	templateDesc.SampleMask = UINT_MAX;
	templateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	templateDesc.NumRenderTargets = 2u;
	templateDesc.RTVFormats[0] = m_backBufferFormat;
	templateDesc.RTVFormats[1] = m_backBufferFormat;
	templateDesc.SampleDesc.Count = m_msaaState ? 4u : 1u;
	templateDesc.SampleDesc.Quality = m_msaaState ? (m_msaaQuality - 1u) : 0u;
	templateDesc.DSVFormat = m_depthStencilFormat;

	gBuffer->InitPSO(templateDesc);
	m_skybox->InitPSO(m_d3dDevice.Get(), templateDesc);
	m_dynamicCube->InitPSO(templateDesc);
	m_shadow->InitPSO(templateDesc);
	m_blur->InitPSO(templateDesc);
	m_TemporalAA->InitPSO(templateDesc);
	m_toneMap->InitPSO(templateDesc);
	m_renderer->InitPSO(templateDesc);
	m_ssao->InitPSO(templateDesc);
}

void BoxApp::CreateFrameResources()
{
	for (auto i = 0; i < frameResourcesCount; ++i)
	{
		m_frameCBuffer.emplace_back(std::make_unique<FrameResource>(m_d3dDevice.Get(), PassConstant::GetPassCount(), RenderItem::GetInstanceCount(), static_cast<UINT>(Material::GetMatSize())));
	}
}

void BoxApp::CreateRenderItems()
{
	auto skybox = std::make_unique<RenderItem>();
	skybox->EmplaceBack();
	skybox->transformPack[0]->m_scale = std::move(XMFLOAT3(5000.0f, 5000.0f, 5000.0f));
	skybox->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skybox->m_matIndex = m_material->m_data["Skybox"]->materialCBIndex;
	skybox->m_type = m_material->m_data["Skybox"]->type;
	skybox->m_mesh = m_meshGeos["Total"].get();
	skybox->eboCount = skybox->m_mesh->drawArgs["Sphere"].eboCount;
	skybox->eboStart = skybox->m_mesh->drawArgs["Sphere"].eboStart;
	skybox->vboStart = skybox->m_mesh->drawArgs["Sphere"].vboStart;
	m_renderItems.emplace_back(std::move(skybox));

	auto debug = std::make_unique<RenderItem>();
	debug->EmplaceBack();
	debug->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	debug->m_matIndex = 0;
	debug->m_type = BlendType::debug;
	debug->m_mesh = m_meshGeos["Total"].get();
	debug->eboCount = debug->m_mesh->drawArgs["Debug"].eboCount;
	debug->eboStart = debug->m_mesh->drawArgs["Debug"].eboStart;
	debug->vboStart = debug->m_mesh->drawArgs["Debug"].vboStart;
	m_renderItems.emplace_back(std::move(debug));

	const auto sponzaModel = Models::ObjLoader::instance().GetObj("Sponza/pbr/sponza.obj").value();
	const UINT len = sponzaModel->meshData.size();
	for (UINT i = 0; i < len; ++i)
	{
		auto sponza = std::make_unique<RenderItem>();
		string geoName("sponza" + to_string(i));
		sponza->EmplaceBack();
		sponza->transformPack[0]->m_scale = std::move(XMFLOAT3(0.1f, 0.1f, 0.1f));
		sponza->m_matIndex = sponzaModel->objMat->m_data[sponzaModel->submesh[i].materialName]->materialCBIndex;
		sponza->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sponza->m_type = BlendType::opaque;
		sponza->m_mesh = m_meshGeos["Total"].get();
		sponza->eboStart = m_meshGeos["Total"]->drawArgs[geoName].eboStart;
		sponza->eboCount = m_meshGeos["Total"]->drawArgs[geoName].eboCount;
		sponza->vboStart = m_meshGeos["Total"]->drawArgs[geoName].vboStart;
		m_renderItems.emplace_back(std::move(sponza));
	}
	Models::Scene::sceneBox.Transform(Models::Scene::sceneBox, XMMatrixScalingFromVector(XMVectorSet(0.07f, 0.07f, 0.07f, 1.0f)));

	// ��Ⱦ��͸������
	for (auto& item : m_renderItems)
	{
		m_renderItemLayers[static_cast<UINT>(item->m_type)].emplace_back(item.get());
	}
}

void BoxApp::CreateTextures()
{
	TextureMgr::instance().Init(m_d3dDevice.Get(), m_commandQueue.Get());

	m_skybox->InitStaticTex("Skybox", TexturePath + L"Skybox/grasscube1024.dds");
	[](){
		static char sponza[] = "Sponza/pbr/sponza.obj";
		Models::ObjLoader::instance().CreateObjFromFile<sponza>();
	}();

	m_dynamicCube->InitTexture("CubeMap");
	m_renderer->InitTexture();
	gBuffer->albedoIdx = TextureMgr::instance().RegisterRenderToTexture("gAlbedo");
	gBuffer->depthIdx = TextureMgr::instance().RegisterRenderToTexture("gPos");
	gBuffer->normalIdx = TextureMgr::instance().RegisterRenderToTexture("gNormal");
	m_shadow->InitTexture("ShadowMap");
	m_blur->InitTexture();
	m_toneMap->InitTexture();
	m_ssao->InitTexture();
	m_TemporalAA->InitTexture("TAATex", "prevTex", "TAAMotionVector");
}

void BoxApp::CreateMaterials()
{
	//m_material->CreateMaterial("Sphere", "Stone", Material::GetMatIndex(), 0.3f, XMFLOAT3(0.0f, 0.0f, 0.0f), 0.3f, BlendType::opaque);
	//m_material->CreateMaterial("Cube", "Brick", Material::GetMatIndex(), 0.1f, XMFLOAT3(0.0f, 0.0f, 0.0f), 0.1f, BlendType::opaque);
	//m_material->CreateMaterial("Grid", "Tile", Material::GetMatIndex(), 0.5f, XMFLOAT3(0.0f, 0.0f, 0.0f), 0.2f, BlendType::opaque);
	m_material->CreateMaterial("Skybox", m_skybox->TexName() , 3, 1.0f, XMFLOAT3(0.1f, 0.1f, 0.1f), 0.0f, BlendType::skybox);
}

auto BoxApp::CreateStaticSampler2D() -> std::array<const CD3DX12_STATIC_SAMPLER_DESC, 8>
{
	/*
	 * �˲�����������ֵ�����Բ�ֵģʽ
	 * warp: �ظ�Ѱַģʽ�� border:�߿���ɫѰַ��clamp��ǿ��������ɫ��mirror���߽羵�����
	 */
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2, D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 8);
	const CD3DX12_STATIC_SAMPLER_DESC shadowSampler(6, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0.0f, 16, D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, 0.0f, 8);
	const CD3DX12_STATIC_SAMPLER_DESC depthMapSampler(7, D3D12_FILTER_MIN_MAG_MIP_LINEAR,D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0.0f,0, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE); 

	return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp, shadowSampler, depthMapSampler };
}

void BoxApp::UpdateObjectInstance(const GameTimer& timer)
{
	UINT offset = 0;
	// ����ֻ��Ҫ����һ�ε�����Ӧ���洢�ڳ����������У�ֻ�е����ʶ�仯ʱ�Ż������³���������
	auto currInstanceData = m_currFrameResource->m_uploadCBuffer.get();
	for (const auto& item : m_renderItems)
	{
		item->Update(timer, currInstanceData, offset);
		offset += item->GetInstanceSize();
	}
}

void BoxApp::UpdatePassConstant(const GameTimer& timer)
{
	// ÿһ֡��Ҫ��֡��Դָ��仯���仯�����ݣ���view����Projection�����
	XMStoreFloat4x4(&m_currPassCB.view_gpu, XMMatrixTranspose(m_camera->GetCurrViewXM()));
	XMStoreFloat4x4(&m_currPassCB.proj_gpu, XMMatrixTranspose(m_camera->GetCurrProjXM()));
	XMStoreFloat4x4(&m_currPassCB.vp_gpu, XMMatrixTranspose(m_camera->GetCurrVPXM()));
	XMStoreFloat4x4(&m_currPassCB.invProj_gpu, XMMatrixTranspose(m_camera->GetInvProjXM()));
	XMStoreFloat4x4(&m_currPassCB.shadowTransform_gpu, XMMatrixTranspose(m_shadow->GetShadowViewXM()));
	XMStoreFloat4x4(&m_currPassCB.viewPortRay_gpu, XMMatrixTranspose(m_camera->GetViewPortRayXM()));
	m_currPassCB.nearZ_gpu = m_camera->m_nearPlane;
	m_currPassCB.farZ_gpu = m_camera->m_farPlane;
	m_currPassCB.deltaTime_gpu = static_cast<float>(timer.DeltaTime());
	m_currPassCB.totalTime_gpu = static_cast<float>(timer.TotalTime());
	m_currPassCB.renderTargetSize_gpu = {static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight)};
	m_currPassCB.invRenderTargetSize_gpu = { 1.0f / static_cast<float>(m_clientWidth), 1.0f / static_cast<float>(m_clientHeight)};
	m_currPassCB.cameraPos_gpu = m_camera->GetCurrPos();
	m_currPassCB.lights[0].direction = m_pixelLights[0]->GetData().direction;
	m_currPassCB.lights[0].strength = m_pixelLights[0]->GetData().strength;
	m_currPassCB.lights[1].direction = m_pixelLights[1]->GetData().direction;
	m_currPassCB.lights[1].strength = m_pixelLights[1]->GetData().strength;
	m_currPassCB.lights[2].direction = m_pixelLights[2]->GetData().direction;
	m_currPassCB.lights[2].strength = m_pixelLights[2]->GetData().strength;

	auto passCB = m_currFrameResource->m_passCBuffer.get();
	passCB->Copy(m_passOffset, m_currPassCB);
}

void BoxApp::UpdateMaterialConstant(const GameTimer& timer)
{
	auto currMatConstant = m_currFrameResource->m_materialCBuffer.get();
	m_material->Update(timer, currMatConstant);
	Models::ObjLoader::instance().UpdateAllModels(timer, currMatConstant);
}

void BoxApp::UpdateOffScreen(const GameTimer& timer)
{
	m_shadow->Update(timer, [&](UINT offset, auto& constant) {
		auto passCB = m_currFrameResource->m_passCBuffer.get();
		passCB->Copy(offset, constant);
	});
	m_blur->Update(timer, [](UINT, auto&){});
	m_TemporalAA->Update(timer, [](UINT, auto&){});
}

void BoxApp::UpdatePostProcess(const GameTimer& timer)
{
	PostProcessPass ppp;
	XMStoreFloat4x4(&ppp.view_gpu, XMMatrixTranspose(m_camera->GetCurrViewXM()));
	XMStoreFloat4x4(&ppp.proj_gpu, XMMatrixTranspose(m_camera->GetNonjitteredProjXM()));
	XMStoreFloat4x4(&ppp.vp_gpu, XMMatrixTranspose(m_camera->GetCurrVPXM()));
	XMStoreFloat4x4(&ppp.nonjitteredVP_gpu, XMMatrixTranspose(m_camera->GetNonjitteredCurrVPXM()));
	XMStoreFloat4x4(&ppp.previousVP_gpu, XMMatrixTranspose(m_camera->GetNonJitteredPreviousVPXM()));
	XMStoreFloat4x4(&ppp.invView_gpu, XMMatrixTranspose(m_camera->GetInvViewXM()));
	XMStoreFloat4x4(&ppp.viewPortRay_gpu, XMMatrixTranspose(m_camera->GetViewPortRayXM()));
	ppp.nearZ_gpu = m_camera->m_nearPlane;
	ppp.farZ_gpu = m_camera->m_farPlane;
	ppp.deltaTime_gpu = static_cast<float>(timer.DeltaTime());
	ppp.totalTime_gpu = static_cast<float>(timer.TotalTime());
	ppp.cameraPos_gpu = m_camera->GetCurrPos();

	auto postProcessPass = m_currFrameResource->m_postProcessCBuffer.get();
	postProcessPass->Copy(0, ppp);
}

void BoxApp::UpdateLightPos(const GameTimer& timer)
{
	const auto totalTime = timer.TotalTime();
	for (UINT i = 0; i < pointLightNum; ++i)
	{
		const auto& data = m_pointLightPos[i];
		const float angle = data->angle + totalTime * data->moveSpeed;
		const auto& params = m_computeLights[i];
		XMFLOAT3 pos = XMFLOAT3(data->radius * std::cos(angle), data->height, data->radius * std::sin(angle));
		params->MovePos(pos);
	}
}

void BoxApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& items)
{
	auto objectConstantBuffer = m_currFrameResource->m_uploadCBuffer->GetResource();
	for (auto& item : items)
	{
		const auto& vboView = item->m_mesh->GetVBOView();
		const auto& eboView = item->m_mesh->GetEBOView();
		cmdList->IASetVertexBuffers(0, 1, &vboView);
		cmdList->IASetIndexBuffer(&eboView);
		cmdList->IASetPrimitiveTopology(item->m_topologyType);

		cmdList->SetGraphicsRootShaderResourceView(1, objectConstantBuffer->GetGPUVirtualAddress() + item->instanceStart * sizeof(ObjectInstance));

		cmdList->DrawIndexedInstanced(item->eboCount, item->GetInstanceSize(), item->eboStart, item->vboStart, 0);
	}
}

void BoxApp::DrawPostProcess(ID3D12GraphicsCommandList* cmdList) {
	m_renderer->SetBloomState<0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList);
	m_TemporalAA->Draw(cmdList, [&](UINT){
		cmdList->SetComputeRootDescriptorTable(3, m_renderer->GetBloomSRV<0>());
	});
	m_renderer->SetBloomState<0, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList);

	// ����̨�����������ݿ������Դ�
	// ִ����Ϻ����ݴ�Ĭ�ϻ������������ڴ滺������
	m_blur->Draw(cmdList, [&](UINT) {
		m_renderer->SetBloomState<1, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE>(cmdList);
		cmdList->CopyResource(m_blur->GetResourceDownSampler(), m_renderer->GetBloomRes());
		m_renderer->SetBloomState<1, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList);
	});

	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, m_blur->GetResourceUpSampler());
	m_toneMap->Draw(cmdList, [&](UINT)
	{
		cmdList->SetComputeRootDescriptorTable(1, m_TemporalAA->GetGpuSRV());
		cmdList->SetComputeRootDescriptorTable(3, m_blur->GetUpSamplerSRV());
	});
	ChangeState<D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON>(cmdList, m_blur->GetResourceUpSampler());

	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, GetCurrentBackBuffer());
	cmdList->CopyResource(GetCurrentBackBuffer(), m_toneMap->GetResource());
	m_toneMap->SetResourceState<D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON>(cmdList);

#if defined(DEBUG) || defined(_DEBUG)
	ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList, GetCurrentBackBuffer());
	Debug::DebugMgr::instance().PrepareToDebug(cmdList, GetCurrentBackBufferView());
	cmdList->SetGraphicsRootDescriptorTable(0, m_shadow->GetGpuSRV());
	DrawDebugItems(cmdList);
	ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, GetCurrentBackBuffer());
#endif
	ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT>(cmdList, GetCurrentBackBuffer());
}

void BoxApp::DrawDebugItems(ID3D12GraphicsCommandList* cmdList) const
{
	const auto& items = m_renderItemLayers[static_cast<UINT>(BlendType::debug)];
	for (auto& item : items )
	{
		const auto& vboView = item->m_mesh->GetVBOView();
		const auto& eboView = item->m_mesh->GetEBOView();
		cmdList->IASetVertexBuffers(0, 1, &vboView);
		cmdList->IASetIndexBuffer(&eboView);
		cmdList->IASetPrimitiveTopology(item->m_topologyType);

		cmdList->DrawIndexedInstanced(item->eboCount, item->GetInstanceSize(), item->eboStart, item->vboStart, 0);
	}
}
