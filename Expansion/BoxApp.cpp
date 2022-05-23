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
#if defined(DEBUG) || defined(_DEBUG)
#include "DebugMgr.hpp"
#endif

BoxApp::BoxApp(HINSTANCE instance, bool startMsaa, Camera::CameraType type)
: D3DApp_Template(instance, startMsaa, type), m_material(std::make_shared<Material>()), m_skybox(std::make_unique<Effect::CubeMap>())
{
	m_lights.reserve(maxLights);
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

	// 执行命令队列初始化命令
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

	// 循环获取当前帧资源的数组
	m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % frameResourcesCount;
	m_currFrameResource = m_frame_cBuffer[m_currFrameResourceIndex].get();

	// GPU若还未执行完当前帧资源的所有命令,表明CPU执行过快, CPU就要进入等待状态直到GPU完成命令并到达围栏点
	if (m_currFrameResource->m_fence != 0 && m_fence->GetCompletedValue() < m_currFrameResource->m_fence)
	{
		HANDLE eventHandler = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		m_fence->SetEventOnCompletion(m_currFrameResource->m_fence, eventHandler);
		WaitForSingleObject(eventHandler, INFINITE);
		CloseHandle(eventHandler);
	}

	{
		XMMATRIX rotate = XMMatrixRotationY(static_cast<float>(0.1 * timer.DeltaTime()));
		XMVECTOR lightDir = XMVector3TransformNormal(m_lights[0]->GetLightDir(), rotate);
		XMStoreFloat3(&const_cast<XMFLOAT3&>(m_lights[0]->GetData().direction), lightDir);
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

	// 将根签名和CBV/SRV堆设置到命令队列上
	ID3D12DescriptorHeap* descriptorSRVHeaps[] = { TextureMgr::instance().GetSRVDescriptorHeap() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorSRVHeaps), descriptorSRVHeaps);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// matBuffer传递
	auto matBuffer = m_currFrameResource->m_materialCBuffer->GetResource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	// textureBuffer传递
	m_commandList->SetGraphicsRootDescriptorTable(6, TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());

	/*
	 * 实时离屏渲染绘制
	 */
	m_shadow->Draw(m_commandList.Get(), [&](UINT offset)
	{
		constexpr UINT passCBSize = D3DUtil::AlignsConstantBuffer(sizeof(PassConstant));
		auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
		auto address = passCB->GetGPUVirtualAddress() + 7 * passCBSize;
		m_commandList->SetGraphicsRootConstantBufferView(0, address);
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
	});

	m_commandList->RSSetViewports(1, &m_camera->GetViewPort());
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	ChangeState<D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET>(m_commandList.Get(), GetCurrentBackBuffer());
	gBuffer->RefreshGBuffer(m_commandList.Get());
	m_commandList->ClearRenderTargetView(GetCurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	m_commandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,0.0f, 0, 0, nullptr);

	// 指定渲染缓冲区
	{
		const auto& depthStencilView = GetDepthStencilView();
		m_commandList->OMSetRenderTargets(3, &gBuffer->gBufferRTV[0], true, &depthStencilView);
	}

	// 通过传参的方式将CBV和某个根描述符相互绑定
	m_commandList->SetPipelineState(gBuffer->m_pso.Get());
	auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
	auto address = passCB->GetGPUVirtualAddress();
	m_commandList->SetGraphicsRootConstantBufferView(0, address);
	DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);

	for (int i = 0; i < 3; ++i)
	{
		ChangeState<D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(m_commandList.Get(), gBuffer->gBufferRes[i].Get());
	}
	// 向Post Process中传递GBuffer和PassConstant
	// 向GPU中传递GBuffer纹理
	auto gBufferSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	gBufferSRVHandler.Offset(gBuffer->albedoIdx, m_cbvUavDescriptorSize);
	PostProcessMgr::instance().UpdateResources<PostProcessMgr::Compute>(m_commandList.Get(), m_currFrameResource->m_postProcessCBuffer->GetResource()->GetGPUVirtualAddress(), gBufferSRVHandler);
	m_commandList->SetGraphicsRootDescriptorTable(3, gBufferSRVHandler);

	// SSAO绘制
	m_ssao->Draw(m_commandList.Get(), [](UINT) {});
	auto ssaoSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	ssaoSRVHandler.Offset(m_ssao->GetSrvIdx("SSAO").value_or(0), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(4, ssaoSRVHandler);

	// 向GPU中传递shadow纹理
	auto shadowSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	shadowSRVHandler.Offset(m_shadow->GetSrvIdx("ShadowMap").value_or(0), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(5, shadowSRVHandler);
	auto skyboxSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	skyboxSRVHandler.Offset(m_skybox->GetStaticID(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(7, skyboxSRVHandler);

	m_renderer->Draw(m_commandList.Get(), [](UINT){});
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
	// 按照资源的用途指示其状态的转变，将渲染目标状态转换为呈现状态
	DrawPostProcess(m_commandList.Get());

	// 完成命令的记录
	ThrowIfFailed(m_commandList->Close());
	// 将命令队列中添加欲执行的命令列表
	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	// 交换前后台缓冲区
	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currBackBuffer = (m_currBackBuffer + 1) % m_swapBufferCount;
	// 通过帧资源替换FlushCommandQueue,实际上无法完全避免等待状况的发生，但是命令队列保持非空，使得GPU总有任务执行
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
		// 根据鼠标的移动距离计算旋转角度，令每个像素都按此角度的0.25倍s旋转
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

	m_camera->SetJitter(m_TemporalAA->GetPrevJitter(), m_TemporalAA->GetJitter());
	m_camera->Update();
}

void BoxApp::RegisterRTVAndDSV() {
	RtvDsvMgr::instance().RegisterRTV(m_swapBufferCount );
	RtvDsvMgr::instance().RegisterDSV(1);
}

void BoxApp::CreateOffScreenRendering() {
	Models::ObjLoader::instance().Init(m_d3dDevice.Get(), m_commandList.Get());
	PostProcessMgr::instance().Init(m_d3dDevice.Get());
	gBuffer = std::make_unique<Renderer::GBuffer>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R16G16B16A16_SNORM);
	m_renderer = std::make_unique<Renderer::DeferShading>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, m_backBufferFormat);
	m_shadow = std::make_unique<Effect::Shadow>(m_d3dDevice.Get(), 2048U, DXGI_FORMAT_R24G8_TYPELESS);
	m_dynamicCube = std::make_unique<Effect::DynamicCubeMap>(m_d3dDevice.Get(), 1024U, 1024U, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_dynamicCube->InitCamera(0.0f, 2.0f, 0.0f);
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
		constant.lights[0].direction = m_lights[0]->GetData().direction;
		constant.lights[0].strength = m_lights[0]->GetData().strength;
		constant.lights[1].direction = m_lights[1]->GetData().direction;
		constant.lights[1].strength = m_lights[1]->GetData().strength;
		constant.lights[2].direction = m_lights[2]->GetData().direction;
		constant.lights[2].strength = m_lights[2]->GetData().strength;

		auto currPass = m_currFrameResource->m_passCBuffer.get();
		currPass->Copy(1 + i, constant);
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

	// 将根签名和CBV/SRV堆设置到命令队列上
	ID3D12DescriptorHeap* descriptorSRVHeaps[] = { TextureMgr::instance().GetSRVDescriptorHeap() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorSRVHeaps), descriptorSRVHeaps);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// matBuffer传递
	auto matBuffer = m_currFrameResource->m_materialCBuffer->GetResource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	// textureBuffer传递
	m_commandList->SetGraphicsRootDescriptorTable(6, TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	// 向GPU中传递skybox纹理
	auto skyboxSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	skyboxSRVHandler.Offset(m_skybox->GetStaticID(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(7, skyboxSRVHandler);

	// LUT Shadow
	m_shadow->Draw(m_commandList.Get(), [&](UINT offset) {
			constexpr UINT passCBSize = D3DUtil::AlignsConstantBuffer(sizeof(PassConstant));
			auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
			auto address = passCB->GetGPUVirtualAddress() + 7 * passCBSize;
			m_commandList->SetGraphicsRootConstantBufferView(0, address);
			DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
	});

	// 向GPU中传递shadow纹理
	auto shadowSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	shadowSRVHandler.Offset(m_shadow->GetSrvIdx("ShadowMap").value_or(0), m_cbvUavDescriptorSize);
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


	// TAA prev Draw
	m_TemporalAA->FirstDraw(m_commandList.Get(), GetDepthStencilView(), [&](){
		m_commandList->SetPipelineState(LUTParams.Get());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
		m_commandList->SetPipelineState(m_skybox->GetPSO());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::skybox)]);
	});

	// 完成命令的记录
	ThrowIfFailed(m_commandList->Close());
	// 将命令队列中添加欲执行的命令列表
	ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	// 交换前后台缓冲区
	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currBackBuffer = (m_currBackBuffer + 1) % m_swapBufferCount;
	FlushCommandQueue();
}

void BoxApp::CreateLights()
{
	LightData dirLight0;
	dirLight0.direction = { 0.57735f, -0.57735f, 0.57735f };
	dirLight0.strength = { 0.8f, 0.8f, 0.8f };
	m_lights.emplace_back(std::make_shared<Light>(std::move(dirLight0)));
	LightData dirLight1;
	dirLight1.direction = { -0.57735f, -0.57735f, 0.57735f };
	dirLight1.strength = { 0.4f, 0.4f, 0.4f };
	m_lights.emplace_back(std::make_shared<Light>(std::move(dirLight1)));
	LightData dirLight2;
	dirLight2.direction = { 0.0f, -0.707f, -0.707f };
	dirLight2.strength = { 0.2f, 0.2f, 0.2f };
	m_lights.emplace_back(std::make_shared<Light>(std::move(dirLight2)));
	m_shadow->RegisterMainLight(m_lights[0].get());
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
	m_renderer->InitDSV(GetDepthStencilView());
	m_renderer->CreateDescriptors(cpuSrvStart, cpuRtvStart, cpuDsvStart, gpuSrvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize, m_dsvDescriptorSize);
	m_ssao->CreateRandomTexture(m_commandList.Get());
	m_ssao->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuRtvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize);
	m_TemporalAA->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuRtvStart, m_cbvUavDescriptorSize, m_rtvDescriptorSize);
}

// 根签名：执行绘制命令之前，应用成程序将绑定到流水线上的资源映射到对应的输入寄存器。
// 根签名需要尽可能小，因为根签名越大，则根实参的快照也就越大同时为了避免频繁切换根签名。要将根参数按照变更频率由高到低排列
void BoxApp::CreateRootSignature()
{
	//CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	//// 表中描述符的数量, 绑定的基准着色器寄存器位置
	//cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	//CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	//cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	CD3DX12_DESCRIPTOR_RANGE skyboxSRV;
	skyboxSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	CD3DX12_DESCRIPTOR_RANGE texSRV;
	texSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 256, 3, 0);
	CD3DX12_DESCRIPTOR_RANGE shadowSRV;
	shadowSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE gBufferSRV;
	gBufferSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 3, 1);
	CD3DX12_DESCRIPTOR_RANGE randSRV;
	randSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	CD3DX12_DESCRIPTOR_RANGE ssaoSRV;
	ssaoSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 1);
	CD3DX12_ROOT_PARAMETER parameters[11]{};
	parameters[0].InitAsConstantBufferView(0); // 渲染过程的CBV
	parameters[1].InitAsShaderResourceView(1, 1); // 物体的CBV
	parameters[2].InitAsShaderResourceView(0, 1); // 物体材质的结构化缓冲区
	parameters[3].InitAsDescriptorTable(1, &gBufferSRV, D3D12_SHADER_VISIBILITY_PIXEL); // gBuffer的位置
	parameters[4].InitAsDescriptorTable(1, &ssaoSRV, D3D12_SHADER_VISIBILITY_PIXEL);
	parameters[5].InitAsDescriptorTable(1, &shadowSRV, D3D12_SHADER_VISIBILITY_PIXEL);// 阴影贴图的位置
	parameters[6].InitAsDescriptorTable(1, &texSRV, D3D12_SHADER_VISIBILITY_PIXEL); // 所有纹理的位置
	parameters[7].InitAsDescriptorTable(1, &skyboxSRV, D3D12_SHADER_VISIBILITY_PIXEL); // 天空盒的位置
	parameters[8].InitAsDescriptorTable(1, &randSRV, D3D12_SHADER_VISIBILITY_PIXEL); // SSAO随机转动纹理
	parameters[9].InitAsConstantBufferView(1); // SSAO ConstantBuffer纹理
	parameters[10].InitAsConstantBufferView(2); // Bilateral Blur 纹理

	// 静态采样器设置
	auto staticSampler = CreateStaticSampler2D();
	// 根签名的参数组成
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(11U, parameters, staticSampler.size(), staticSampler.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	// 创建只含有一个常量缓冲区组成的描述符区域的根签名
	ComPtr<ID3DBlob> serialRootSig{ nullptr }; // ID3DBlob是一个普通的内存块，可以返回一个void*数据或返回缓冲区的大小
	ComPtr<ID3DBlob> error{ nullptr };
	HRESULT res = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, serialRootSig.GetAddressOf(), error.GetAddressOf());
	if (error) {
		OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
	}
	ThrowIfFailed(res);
	m_d3dDevice->CreateRootSignature(0, serialRootSig->GetBufferPointer(), 
		serialRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
}

void BoxApp::CreateShadersAndInput() {
	/// <summary>
	/// make_unique是个函数模板，只能推导传递给对象构造函数的参数类型而花括号列表是不可推导的
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
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} })));
	m_ssao->InitShader();
	m_TemporalAA->InitShader(L"Shaders\\TemporalAA_Aliasing", L"Shaders\\MotionVector");
}

void BoxApp::CreateGeometry()
{
	auto totalGeo = std::make_unique<Mesh>();
	BaseMeshData sphere = BaseGeometry::CreateSphere(0.5f, 20, 20);
	BaseMeshData quad = BaseGeometry::CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	///*
	// * 将所有的几个体合并到一堆大的顶点、索引缓冲区中
	//*/
	UINT vboOffset = 0;
	UINT eboOffset = 0;
	// 定义的多个submeshGeometry包括了顶点/索引缓冲区内不同几何体的子网格数据
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
	// 提取所有的顶点元素并方式一个顶点缓冲区
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

// 流水线状态对象将输入布局描述堆、顶点着色器、像素着色器和光栅器状态组绑定到图形流水线上 
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
		m_frame_cBuffer.emplace_back(std::make_unique<FrameResource>(m_d3dDevice.Get(), 8, RenderItem::GetInstanceCount(), static_cast<UINT>(Material::GetMatSize())));
	}
}

void BoxApp::CreateRenderItems()
{
	//auto box = std::make_unique<RenderItem>();
	//box->EmplaceBack(std::move(XMFLOAT3(0.0f, 2.0f, 0.0f)), std::move(XMFLOAT3(2.0f, 2.0f, 2.0f)));
	//box->m_mesh = m_meshGeos["Total"].get();
	//box->m_matIndex = m_material->m_data["Cube"]->materialCBIndex;
	//box->m_type = m_material->m_data["Cube"]->type;
	//box->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//box->eboCount = box->m_mesh->drawArgs["Cube"].eboCount;
	//box->eboStart = box->m_mesh->drawArgs["Cube"].eboStart;
	//box->vboStart = box->m_mesh->drawArgs["Cube"].vboStart;
	//m_renderItems.emplace_back(std::move(box));

	//auto grid = std::make_unique<RenderItem>();
	//grid->EmplaceBack(XMFLOAT3(0.0f, 1.0f, 0.0f)), std::move(XMFLOAT3(10.0f, 10.0f, 10.0f));
	//grid->m_mesh = m_meshGeos["Total"].get();
	//grid->m_matIndex = m_material->m_data["Grid"]->materialCBIndex;
	//grid->m_type = m_material->m_data["Grid"]->type;
	//grid->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//grid->eboCount = grid->m_mesh->drawArgs["Grid"].eboCount;
	//grid->eboStart = grid->m_mesh->drawArgs["Grid"].eboStart;
	//grid->vboStart = grid->m_mesh->drawArgs["Grid"].vboStart;
	//m_renderItems.emplace_back(std::move(grid));

	//// 渲染球体
	//auto sphere = make_unique<RenderItem>();
	//sphere->m_mesh = m_meshGeos["Total"].get();
	//sphere->m_matIndex = m_material->m_data["Sphere"]->materialCBIndex;
	//sphere->m_type = m_material->m_data["Sphere"]->type;
	//sphere->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//sphere->eboCount = sphere->m_mesh->drawArgs["Sphere"].eboCount;
	//sphere->eboStart = sphere->m_mesh->drawArgs["Sphere"].eboStart;
	//sphere->vboStart = sphere->m_mesh->drawArgs["Sphere"].vboStart;
	//for (size_t i = 0; i < 5; ++i)
	//{
	//	sphere->EmplaceBack(std::move(XMFLOAT3(-5.0f, 1.5f, -10.0f + i * 5.0f)));
	//	sphere->EmplaceBack(std::move(XMFLOAT3(5.0f, 1.5f, -10.0f + i * 5.0f)));
	//}
	//m_renderItems.emplace_back(std::move(sphere));

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

	// 渲染不透明物体
	for (auto& item : m_renderItems)
	{
		m_renderItemLayers[static_cast<UINT>(item->m_type)].emplace_back(item.get());
	}
}

void BoxApp::CreateTextures()
{
	TextureMgr::instance().Init(m_d3dDevice.Get(), m_commandQueue.Get());

	//TextureMgr::instance().InsertDDSTexture("Brick", TexturePath + L"Common/bricks.dds");
	//TextureMgr::instance().InsertDDSTexture("BrickNorm", TexturePath + L"Common/bricks_nmap.dds");
	//TextureMgr::instance().InsertDDSTexture("Stone", TexturePath + L"Common/bricks2.dds");
	//TextureMgr::instance().InsertDDSTexture("StoneNorm", TexturePath + L"Common/bricks2_nmap.dds");
	//TextureMgr::instance().InsertDDSTexture("Tile", TexturePath + L"Common/checkboard.dds");
	//TextureMgr::instance().InsertDDSTexture("TileNorm", TexturePath + L"Common/checkboard_nmap.dds");
	m_skybox->InitStaticTex("Skybox", TexturePath + L"Skybox/grasscube1024.dds");
	[](){
		static char sponza[] = "Sponza/pbr/sponza.obj";
		Models::ObjLoader::instance().CreateObjFromFile<sponza>();
	}();
	m_renderer->InitTexture();
	gBuffer->albedoIdx = TextureMgr::instance().RegisterRenderToTexture("gAlbedo");
	gBuffer->depthIdx = TextureMgr::instance().RegisterRenderToTexture("gPos");
	gBuffer->normalIdx = TextureMgr::instance().RegisterRenderToTexture("gNormal");

	m_dynamicCube->InitSRV("CubeMap");
	m_shadow->InitSRV("ShadowMap");
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
	 * 滤波器：常量插值和线性插值模式
	 * warp: 重复寻址模式， border:边框颜色寻址，clamp：强化矫正颜色，mirror：边界镜像绘制
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
	// 物体只需要设置一次的数据应当存储在常量缓冲区中，只有当脏标识变化时才会具体更新常量缓冲区
	auto currInstanceData = m_currFrameResource->m_uploadCBuffer.get();
	for (const auto& item : m_renderItems)
	{
		item->Update(timer, currInstanceData, offset);
		offset += item->GetInstanceSize();
	}
}

void BoxApp::UpdatePassConstant(const GameTimer& timer)
{
	// 每一帧都要随帧资源指向变化而变化的数据，如view矩阵、Projection矩阵等
	XMStoreFloat4x4(&m_currPassCB.view_gpu, XMMatrixTranspose(m_camera->GetCurrViewXM()));
	XMStoreFloat4x4(&m_currPassCB.proj_gpu, XMMatrixTranspose(m_camera->GetCurrProjXM()));
	XMStoreFloat4x4(&m_currPassCB.vp_gpu, XMMatrixTranspose(m_camera->GetCurrVPXM()));
	XMStoreFloat4x4(&m_currPassCB.invProj_gpu, XMMatrixTranspose(m_camera->GetInvProjXM()));
	XMStoreFloat4x4(&m_currPassCB.shadowTransform_gpu, XMMatrixTranspose(m_shadow->GetShadowTransformXM()));
	XMStoreFloat4x4(&m_currPassCB.viewPortRay_gpu, XMMatrixTranspose(m_camera->GetViewPortRayXM()));
	m_currPassCB.nearZ_gpu = m_camera->m_nearPlane;
	m_currPassCB.farZ_gpu = m_camera->m_farPlane;
	m_currPassCB.deltaTime_gpu = static_cast<float>(timer.DeltaTime());
	m_currPassCB.totalTime_gpu = static_cast<float>(timer.TotalTime());
	m_currPassCB.renderTargetSize_gpu = {static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight)};
	m_currPassCB.invRenderTargetSize_gpu = { 1.0f / static_cast<float>(m_clientWidth), 1.0f / static_cast<float>(m_clientHeight)};
	m_currPassCB.cameraPos_gpu = m_camera->GetCurrPos();
	m_currPassCB.lights[0].direction = m_lights[0]->GetData().direction;
	m_currPassCB.lights[0].strength = m_lights[0]->GetData().strength;
	m_currPassCB.lights[1].direction = m_lights[1]->GetData().direction;
	m_currPassCB.lights[1].strength = m_lights[1]->GetData().strength;
	m_currPassCB.lights[2].direction = m_lights[2]->GetData().direction;
	m_currPassCB.lights[2].strength = m_lights[2]->GetData().strength;

	auto passCB = m_currFrameResource->m_passCBuffer.get();
	passCB->Copy(0, m_currPassCB);
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
		passCB->Copy(7, constant);
	});
	m_blur->Update(timer, [](UINT, auto&){});
	m_TemporalAA->Update(timer, [](UINT, auto&){});
}

void BoxApp::UpdatePostProcess(const GameTimer& timer)
{
	PostProcessPass ppp;
	XMStoreFloat4x4(&ppp.view_gpu, XMMatrixTranspose(m_camera->GetCurrViewXM()));
	XMStoreFloat4x4(&ppp.proj_gpu, XMMatrixTranspose(m_camera->GetCurrProjXM()));
	XMStoreFloat4x4(&ppp.vp_gpu, XMMatrixTranspose(m_camera->GetCurrVPXM()));
	XMStoreFloat4x4(&ppp.previousVP_gpu, XMMatrixTranspose(m_camera->GetPreviousVPXM()));
	XMStoreFloat4x4(&ppp.invProj_gpu, XMMatrixTranspose(m_camera->GetInvProjXM()));
	XMStoreFloat4x4(&ppp.viewPortRay_gpu, XMMatrixTranspose(m_camera->GetViewPortRayXM()));
	ppp.nearZ_gpu = m_camera->m_nearPlane;
	ppp.farZ_gpu = m_camera->m_farPlane;
	ppp.deltaTime_gpu = static_cast<float>(timer.DeltaTime());
	ppp.totalTime_gpu = static_cast<float>(timer.TotalTime());
	ppp.cameraPos_gpu = m_camera->GetCurrPos();

	auto postProcessPass = m_currFrameResource->m_postProcessCBuffer.get();
	postProcessPass->Copy(0, ppp);
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
		//// 为了绘制当前的帧资源和物体需要对偏移符堆做偏移
		//UINT cbvIndex = m_currFrameResourceIndex * static_cast<UINT>(items.size()) + item->m_constantBufferIndex;
		//auto cbvHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
		//cbvHandler.Offset(cbvIndex, m_cbvUavDescriptorSize);

		//cmdList->SetGraphicsRootDescriptorTable(0, cbvHandler);
		cmdList->DrawIndexedInstanced(item->eboCount, item->GetInstanceSize(), item->eboStart, item->vboStart, 0);
	}
}

void BoxApp::DrawPostProcess(ID3D12GraphicsCommandList* cmdList) {
	m_renderer->SetBloomState<0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList);
	m_TemporalAA->Draw(cmdList, [&](UINT){
		cmdList->SetComputeRootDescriptorTable(1, m_renderer->GetBloomSRV<0>());
	});
	m_renderer->SetBloomState<0, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET>(cmdList);

	// 将后台缓冲区的数据拷贝到显存
	// 执行完毕后将数据从默认缓冲区拷贝到内存缓冲区中
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
	cmdList->SetGraphicsRootDescriptorTable(0, m_TemporalAA->GetMotionVector());
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
