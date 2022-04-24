#include "BoxApp.h"

#include <DirectXColors.h>
#include <vector>
#include <DirectXMath.h>
#include <algorithm>
#include <memory>
#include <xstring>
#include <dxgi1_6.h>
#include <d3dcommon.h>
#include "BaseGeometry.h"
#include "Texture.h"
#include <ObjLoader.h>

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

	UpdateConstantBuffer(timer);
	UpdateMaterialConstant(timer);
	UpdatePassConstant(timer);
	UpdateOffScreen(timer);
}

void BoxApp::DrawScene(const GameTimer& timer)
{
	auto alloc = m_currFrameResource->m_commandAllocator;
	ThrowIfFailed(alloc->Reset());
	ThrowIfFailed(m_commandList->Reset(alloc.Get(), m_pso["Opaque"].Get()));

	// 将根签名和CBV/SRV堆设置到命令队列上
	ID3D12DescriptorHeap* descriptorSRVHeaps[] = { TextureMgr::instance().GetSRVDescriptorHeap() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorSRVHeaps), descriptorSRVHeaps);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// matBuffer传递
	auto matBuffer = m_currFrameResource->m_materialCBuffer->GetResource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	// textureBuffer传递
	m_commandList->SetGraphicsRootDescriptorTable(3, TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	// 向GPU中传递skybox纹理
	auto skyboxSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	skyboxSRVHandler.Offset(m_skybox->GetStaticID(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(4, skyboxSRVHandler);

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
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(
			GetCurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &trans);
	}

	m_commandList->ClearRenderTargetView(GetCurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	m_commandList->ClearDepthStencilView(GetDepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	// 指定渲染缓冲区
	{
		const auto& backView = GetCurrentBackBufferView();
		const auto& depthStencilView = GetDepthStencilView();
		m_commandList->OMSetRenderTargets(1, &backView, true, &depthStencilView);
	}

	//ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
	//m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//int passIndex = m_cbvOffset + m_currFrameResourceIndex;

	//auto passCBVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
	//passCBVHandler.Offset(passIndex, m_cbvUavDescriptorSize);
	//m_commandList->SetGraphicsRootDescriptorTable(1, passCBVHandler);
	auto shadowSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	shadowSRVHandler.Offset(m_shadow->GetSrvIdx(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(5, shadowSRVHandler);

	/*
	if (!m_renderItemLayers[static_cast<UINT>(BlendType::reflective)].empty())
	{
		// 对于需要离屏渲染的物体保修但对设置两个常量缓冲区，一个存储物体镜像另一个则存储光照镜像
		auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
		constexpr UINT currPassByteSize = D3DUtil::AlignsConstantBuffer(sizeof(PassConstant));
		m_commandList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress() + currPassByteSize);
		m_commandList->SetPipelineState(m_pso["Reflective"].Get());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::reflective)]);
		m_commandList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
		m_commandList->OMSetStencilRef(0);
	}
	
	if (!m_renderItemLayers[static_cast<UINT>(BlendType::alpha)].empty())
	{
		m_commandList->SetPipelineState(m_pso["Alpha"].Get());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::alpha)]);
	}

	if (!m_renderItemLayers[static_cast<UINT>(BlendType::mirror)].empty())
	{
		m_commandList->OMSetStencilRef(1);
		m_commandList->SetPipelineState(m_pso["Mirror"].Get());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::mirror)]);
	}

	if (!m_renderItemLayers[static_cast<UINT>(BlendType::transparent)].empty())
	{
		m_commandList->SetPipelineState(m_pso["Transparent"].Get());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::transparent)]);
	}
	*/

	// 通过传参的方式将CBV和某个根描述符相互绑定
	{
		m_commandList->SetPipelineState(m_pso["Opaque"].Get());
		auto passCB = m_currFrameResource->m_passCBuffer->GetResource(); 
		m_commandList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]); 
	}
	m_commandList->SetPipelineState(m_skybox->GetPSO());
	DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::skybox)]);

	// 按照资源的用途指示其状态的转变，将渲染目标状态转换为呈现状态
	{
		const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(
			GetCurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &trans);
	}
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
		m_camera->MoveForward(10.0f * delta);
	if (GetAsyncKeyState('S') & 0x8000)
		m_camera->MoveForward(-10.0f * delta);
	if (GetAsyncKeyState('A') & 0x8000)
		m_camera->Strafe(-10.0f * delta);
	if (GetAsyncKeyState('D') & 0x8000)
		m_camera->Strafe(10.0f * delta);

	m_camera->Update();
}

void BoxApp::CreateRtvAndDsvDescriptorHeaps() {
	// 为立方体渲染目标添加六个RTV
	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDesc.NumDescriptors = m_swapBufferCount + 6;
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDesc.NodeMask = 0;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())));
	// 新增个两个DSV
	D3D12_DESCRIPTOR_HEAP_DESC dsvDesc;
	dsvDesc.NumDescriptors = 3;
	dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDesc.NodeMask = 0;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
	m_dynamicCube->InitDSV({ m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_dsvDescriptorSize });
}

void BoxApp::CreateOffScreenRendering() {
	m_shadow = std::make_unique<Effect::Shadow>(m_d3dDevice.Get(), 2048U, DXGI_FORMAT_R24G8_TYPELESS);
	m_dynamicCube = std::make_unique<Effect::DynamicCubeMap>(m_d3dDevice.Get(), 1024U, 1024U, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_dynamicCube->InitCamera(0.0f, 2.0f, 0.0f);
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
	auto alloc = m_currFrameResource->m_commandAllocator;
	ThrowIfFailed(alloc->Reset());
	ThrowIfFailed(m_commandList->Reset(alloc.Get(), m_pso["Opaque"].Get()));

	// 将根签名和CBV/SRV堆设置到命令队列上
	ID3D12DescriptorHeap* descriptorSRVHeaps[] = { TextureMgr::instance().GetSRVDescriptorHeap() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorSRVHeaps), descriptorSRVHeaps);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// matBuffer传递
	auto matBuffer = m_currFrameResource->m_materialCBuffer->GetResource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	// textureBuffer传递
	m_commandList->SetGraphicsRootDescriptorTable(3, TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	// 向GPU中传递skybox纹理
	auto skyboxSRVHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureMgr::instance().GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	skyboxSRVHandler.Offset(m_skybox->GetStaticID(), m_cbvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(4, skyboxSRVHandler);

	// LUT drawing
	m_dynamicCube->Draw(m_commandList.Get(), [&](UINT offset) {
		constexpr auto passCBSize = D3DUtil::AlignsConstantBuffer(sizeof(PassConstant));
		auto passCB = m_currFrameResource->m_passCBuffer->GetResource();
		auto address = passCB->GetGPUVirtualAddress() + (offset + 1) * passCBSize;
		m_commandList->SetGraphicsRootConstantBufferView(0, address);
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::opaque)]);
		m_commandList->SetPipelineState(m_skybox->GetPSO());
		DrawRenderItems(m_commandList.Get(), m_renderItemLayers[static_cast<UINT>(BlendType::skybox)]);
		m_commandList->SetPipelineState(m_pso["Opaque"].Get());
	});

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
	auto cpuRtvStart = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	UINT rtvOffset = m_swapBufferCount;
	m_dynamicCube->CreateDescriptors(cpuSrvStart, gpuSrvStart, cpuRtvStart, rtvOffset, m_cbvUavDescriptorSize, m_rtvDescriptorSize);
	m_dynamicCube->InitDepthAndStencil(m_commandList.Get());
	// TODO: shadow Descriptor heaps
	m_shadow->CreateDescriptors(cpuSrvStart, gpuSrvStart, dsvCpuStart, m_cbvUavDescriptorSize, m_dsvDescriptorSize, 2);
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
	texSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 256, 2, 0);
	CD3DX12_DESCRIPTOR_RANGE shadowSRV;
	shadowSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	CD3DX12_ROOT_PARAMETER parameters[6]{};
	parameters[0].InitAsConstantBufferView(0); // 渲染过程的CBV
	parameters[1].InitAsConstantBufferView(1); // 物体的CBV
	parameters[2].InitAsShaderResourceView(0, 1); // 物体材质的结构化缓冲区
	parameters[3].InitAsDescriptorTable(1, &texSRV, D3D12_SHADER_VISIBILITY_PIXEL); // 所有纹理的位置
	parameters[4].InitAsDescriptorTable(1, &skyboxSRV, D3D12_SHADER_VISIBILITY_PIXEL); // 天空盒的位置
	parameters[5].InitAsDescriptorTable(1, &shadowSRV, D3D12_SHADER_VISIBILITY_PIXEL);// 阴影贴图的位置

	// 静态采样器设置
	auto staticSampler = std::move(CreateStaticSampler2D());
	// 根签名的参数组成
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(6U, parameters, staticSampler.size(), staticSampler.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	// 创建只含有一个常量缓冲区组成的描述符区域的根签名
	ComPtr<ID3DBlob> serialRootSig{ nullptr }; // ID3DBlob是一个普通的内存块，可以返回一个void*数据或返回缓冲区的大小
	ComPtr<ID3DBlob> error{ nullptr };
	HRESULT res = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, serialRootSig.GetAddressOf(), error.GetAddressOf());
	if (error) {
		::OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
	}
	ThrowIfFailed(res);
	m_d3dDevice->CreateRootSignature(0, serialRootSig->GetBufferPointer(), 
		serialRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
}

void BoxApp::CreateShadersAndInput()
{
	/// <summary>
	/// make_unique是个函数模板，只能推导传递给对象构造函数的参数类型而花括号列表是不可推导的
	/// </summary>
	m_shader["Opaque"] = make_unique<Shader>(default_shader, L"Shaders\\Box",
		initializer_list<D3D12_INPUT_ELEMENT_DESC>({{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}}));
	//m_shader["Alpha"] = make_unique<Shader>(default_shader, L"Shaders\\Alpha",
	//	initializer_list<D3D12_INPUT_ELEMENT_DESC>({ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//	{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} }));
	//m_shader["Reflect"] = make_unique<Shader>(default_shader, L"Shaders\\Reflect",
	//	initializer_list<D3D12_INPUT_ELEMENT_DESC>({ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} }));
	m_shadow->InitShader(L"Shaders\\Shadow");
	m_dynamicCube->InitShader(L"Shaders\\Skybox");
}

void BoxApp::CreateGeometry()
{
	BaseMeshData box = BaseGeometry::CreateCube(1.0f, 1.0f, 1.0f, 3);
	BaseMeshData sphere = BaseGeometry::CreateSphere(0.5f, 20, 20);
	BaseMeshData grid = BaseGeometry::CreateGrid(20.0f, 30.0f, 60, 40);

	/*
	 * 将所有的几个体合并到一堆大的顶点、索引缓冲区中
	 */
	UINT boxVBOOffset = 0;
	UINT sphereVBOOffset = static_cast<UINT>(box.VBOs.size());
	UINT gridVBOOffset = sphereVBOOffset + static_cast<UINT>(sphere.VBOs.size());

	UINT boxEBOOffset = 0;
	UINT sphereEBOOffset = static_cast<UINT>(box.EBOs.size());
	UINT gridEBOOffset = sphereEBOOffset + static_cast<UINT>(sphere.EBOs.size());
	// 定义的多个submeshGeometry包括了顶点/索引缓冲区内不同几何体的子网格数据
	Submesh boxSubmesh, sphereSubmesh, gridSubmesh;
	boxSubmesh.eboCount = static_cast<UINT>(box.EBOs.size());
	boxSubmesh.eboStart = boxEBOOffset;
	boxSubmesh.vboStart = boxVBOOffset;
	sphereSubmesh.eboCount = static_cast<UINT>(sphere.EBOs.size());
	sphereSubmesh.eboStart = sphereEBOOffset;
	sphereSubmesh.vboStart = sphereVBOOffset;
	gridSubmesh.eboCount = static_cast<UINT>(grid.EBOs.size());
	gridSubmesh.eboStart = gridEBOOffset;
	gridSubmesh.vboStart = gridVBOOffset;

	// 提取所有的顶点元素并方式一个顶点缓冲区
	auto vertSize = box.VBOs.size() + sphere.VBOs.size() + grid.VBOs.size();
	std::vector<Vertex_GPU> vbo;
	vbo.reserve(vertSize);

	for (auto i = 0; i < box.VBOs.size(); ++i)
	{
		vbo.emplace_back(Vertex_GPU{box.VBOs[i].pos, box.VBOs[i].normal, box.VBOs[i].tangent, box.VBOs[i].tex});
	}
	for (auto i = 0; i < sphere.VBOs.size(); ++i)
	{
		vbo.emplace_back(Vertex_GPU{sphere.VBOs[i].pos, sphere.VBOs[i].normal, sphere.VBOs[i].tangent, sphere.VBOs[i].tex});
	}
	for (auto i = 0; i < grid.VBOs.size(); ++i)
	{
		vbo.emplace_back(Vertex_GPU{grid.VBOs[i].pos, grid.VBOs[i].normal, grid.VBOs[i].tangent, grid.VBOs[i].tex});
	}
	std::vector<uint16_t> ebo;
	ebo.insert(ebo.end(), std::begin(box.GetEBO_16()), std::end(box.GetEBO_16()));
	ebo.insert(ebo.end(), std::begin(sphere.GetEBO_16()), std::end(sphere.GetEBO_16()));
	ebo.insert(ebo.end(), std::begin(grid.GetEBO_16()), std::end(grid.GetEBO_16()));

	const UINT vboByteSize = static_cast<UINT>(vbo.size()) * static_cast<UINT>(sizeof(Vertex_GPU));
	const UINT eboByteSize = static_cast<UINT>(ebo.size()) * static_cast<UINT>(sizeof(uint16_t));
	auto totalGeo = std::make_unique<Mesh>();
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

	totalGeo->drawArgs["Cube"] = std::move(boxSubmesh);
	totalGeo->drawArgs["Sphere"] = std::move(sphereSubmesh);
	totalGeo->drawArgs["Grid"] = std::move(gridSubmesh);
	m_meshGeos[totalGeo->name] = std::move(totalGeo);
}

// 流水线状态对象将输入布局描述堆、顶点着色器、像素着色器和光栅器状态组绑定到图形流水线上 
void BoxApp::CreatePSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueDesc;
	ZeroMemory(&opaqueDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaqueDesc.InputLayout = { m_shader["Opaque"]->GetInputLayouts(), m_shader["Opaque"]->GetInputLayoutSize() };
	opaqueDesc.pRootSignature = m_rootSignature.Get();
	opaqueDesc.VS = {
		static_cast<BYTE*>(m_shader["Opaque"]->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()),
		m_shader["Opaque"]->GetShaderByType(ShaderPos::vertex)->GetBufferSize()
	};
	opaqueDesc.PS = {
		static_cast<BYTE*>(m_shader["Opaque"]->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),
		m_shader["Opaque"]->GetShaderByType(ShaderPos::fragment)->GetBufferSize()
	};
	opaqueDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaqueDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaqueDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaqueDesc.SampleMask = UINT_MAX;
	opaqueDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaqueDesc.NumRenderTargets = 1u;
	opaqueDesc.RTVFormats[0] = m_backBufferFormat;
	opaqueDesc.SampleDesc.Count = m_msaaState ? 4u : 1u;
	opaqueDesc.SampleDesc.Quality = m_msaaState ? (m_msaaQuality - 1u) : 0u;
	opaqueDesc.DSVFormat = m_depthStencilFormat;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaqueDesc, IID_PPV_ARGS(&m_pso["Opaque"])));

	if (!m_renderItemLayers[static_cast<UINT>(BlendType::transparent)].empty()) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentDesc = opaqueDesc;
		D3D12_RENDER_TARGET_BLEND_DESC transparentBlendDesc;
		transparentBlendDesc.BlendEnable = true; // 选择启用常规混合或逻辑混合
		transparentBlendDesc.LogicOpEnable = false;
		transparentBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; // 指定源混合因子
		transparentBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // 指定目标混合因子
		transparentBlendDesc.BlendOp = D3D12_BLEND_OP_ADD; // 指定RGB混合方式
		transparentBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE; // 指定alpha混合时的源混合因子
		transparentBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO; // 指定alpha混合时的目标混合因子
		transparentBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; // 指定Alpha混合方式
		transparentBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP; // 指定逻辑混合方式
		transparentBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // 指定颜色遮罩
		transparentDesc.BlendState.RenderTarget[0] = std::move(transparentBlendDesc);
		ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&transparentDesc, IID_PPV_ARGS(&m_pso["Transparent"])));
	}

	// 设置alpha测试的描述
	if (!m_renderItemLayers[static_cast<UINT>(BlendType::alpha)].empty()) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaDesc = opaqueDesc;
		alphaDesc.VS = {
			static_cast<BYTE*>(m_shader["Alpha"]->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()),
			m_shader["Alpha"]->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
		alphaDesc.PS = {
			static_cast<BYTE*>(m_shader["Alpha"]->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),
		m_shader["Alpha"]->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
		alphaDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&alphaDesc, IID_PPV_ARGS(&m_pso["Alpha"])));
	}

	//// 设置镜像的模板状态和深度状态
	//if (!m_renderItemLayers[static_cast<UINT>(BlendType::mirror)].empty())
	//{
	//	// 禁止对渲染目标执行写入操作
	//	CD3DX12_BLEND_DESC mirrorBlendState(D3D12_DEFAULT);
	//	D3D12_DEPTH_STENCIL_DESC mirrorStencilDesc;
	//	mirrorStencilDesc.DepthEnable = true;
	//	mirrorStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	//	mirrorStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//	mirrorStencilDesc.StencilEnable = true;
	//	mirrorStencilDesc.StencilReadMask = 0xff;
	//	mirrorStencilDesc.StencilWriteMask = 0xff;
	//	mirrorStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	//	mirrorStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	//	mirrorStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	//	mirrorStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	//	// 暂时不考虑背面朝向的多边形
	//	mirrorStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	//	mirrorStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	//	mirrorStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	//	mirrorStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	//	D3D12_GRAPHICS_PIPELINE_STATE_DESC mirrorDesc = opaqueDesc;
	//	mirrorDesc.BlendState = mirrorBlendState;
	//	mirrorDesc.DepthStencilState = mirrorStencilDesc;
	//	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&mirrorDesc, IID_PPV_ARGS(&m_pso["Mirror"])));
	//}

	//if (!m_renderItemLayers[static_cast<UINT>(BlendType::reflective)].empty())
	//{
	//	/*
	//	 * 用于渲染模板缓冲中反射镜相的PSO
	//	 */
	//	D3D12_DEPTH_STENCIL_DESC reflectionStencilDesc;
	//	reflectionStencilDesc.DepthEnable = true;
	//	reflectionStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//	reflectionStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//	reflectionStencilDesc.StencilEnable = true;
	//	reflectionStencilDesc.StencilReadMask = 0xff;
	//	reflectionStencilDesc.StencilWriteMask = 0xff;

	//	reflectionStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	//	reflectionStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	//	reflectionStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	//	reflectionStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	//	reflectionStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	//	reflectionStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	//	reflectionStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	//	reflectionStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	//	D3D12_GRAPHICS_PIPELINE_STATE_DESC reflectiveDesc = opaqueDesc;
	//	
	//	reflectiveDesc.VS = {
	//		static_cast<BYTE*>(m_shader["Reflect"]->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()),
	//		m_shader["Reflect"]->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	//	reflectiveDesc.PS = {
	//		static_cast<BYTE*>(m_shader["Reflect"]->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()),
	//	m_shader["Reflect"]->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };

	//	reflectiveDesc.DepthStencilState = reflectionStencilDesc;
	//	reflectiveDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	//	reflectiveDesc.RasterizerState.FrontCounterClockwise = true; // 指定三角形怎样旋转朝向的方向才是正面
	//	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&reflectiveDesc, IID_PPV_ARGS(&m_pso["Reflective"])));
	//}
	
	m_skybox->InitPSO(m_d3dDevice.Get(), opaqueDesc);
	m_dynamicCube->InitPSO(m_d3dDevice.Get(), opaqueDesc);
	m_shadow->InitPSO(m_d3dDevice.Get(), opaqueDesc);
}

void BoxApp::CreateFrameResources()
{
	for (auto i = 0; i < frameResourcesCount; ++i)
	{
		m_frame_cBuffer.emplace_back(std::make_unique<FrameResource>(m_d3dDevice.Get(), 8, static_cast<UINT>(m_renderItems.size()), static_cast<UINT>(m_material->m_data.size())));
	}
}

void BoxApp::CreateRenderItems()
{
	vector<unique_ptr<RenderItem>> reflectItems;
	int objCBIndex = 0;
	
	auto box = std::make_unique<RenderItem>();
	box->m_transform->m_position = std::move(XMFLOAT3(0.0f, 2.0f, 0.0f));
	box->m_transform->m_scale = std::move(XMFLOAT3(2.0f, 2.0f, 2.0f));
	box->m_constantBufferIndex = objCBIndex++;
	box->m_mesh = m_meshGeos["Total"].get();
	box->m_material = m_material->m_data["Cube"].get();
	box->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	box->eboCount = box->m_mesh->drawArgs["Cube"].eboCount;
	box->eboStart = box->m_mesh->drawArgs["Cube"].eboStart;
	box->vboStart = box->m_mesh->drawArgs["Cube"].vboStart;
	m_renderItems.emplace_back(std::move(box));

	auto grid = std::make_unique<RenderItem>();
	grid->m_constantBufferIndex = objCBIndex++;
	grid->m_mesh = m_meshGeos["Total"].get();
	grid->m_material = m_material->m_data["Grid"].get();
	grid->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	grid->eboCount = grid->m_mesh->drawArgs["Grid"].eboCount;
	grid->eboStart = grid->m_mesh->drawArgs["Grid"].eboStart;
	grid->vboStart = grid->m_mesh->drawArgs["Grid"].vboStart;
	m_renderItems.emplace_back(std::move(grid));

	// 渲染球体
	for (size_t i = 0; i < 5; ++i)
	{
		auto leftSphere = make_unique<RenderItem>();
		auto rightSphere = make_unique<RenderItem>();
		leftSphere->m_transform->m_position = std::move(XMFLOAT3(-5.0f, 3.5f, -10.0f + i * 5.0f));
		rightSphere->m_transform->m_position = std::move(XMFLOAT3(5.0f, 3.5f, -10.0f + i * 5.0f));
		leftSphere->m_constantBufferIndex = objCBIndex++;
		rightSphere->m_constantBufferIndex = objCBIndex++;
		leftSphere->m_mesh = m_meshGeos["Total"].get();
		rightSphere->m_mesh = m_meshGeos["Total"].get();
		leftSphere->m_material = m_material->m_data["Sphere"].get();
		rightSphere->m_material = m_material->m_data["Sphere"].get();
		leftSphere->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphere->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphere->eboCount = leftSphere->m_mesh->drawArgs["Sphere"].eboCount;
		rightSphere->eboCount = rightSphere->m_mesh->drawArgs["Sphere"].eboCount;
		leftSphere->eboStart = leftSphere->m_mesh->drawArgs["Sphere"].eboStart;
		rightSphere->eboStart = rightSphere->m_mesh->drawArgs["Sphere"].eboStart;
		leftSphere->vboStart = leftSphere->m_mesh->drawArgs["Sphere"].vboStart;
		rightSphere->vboStart = rightSphere->m_mesh->drawArgs["Sphere"].vboStart;
		m_renderItems.emplace_back(std::move(leftSphere));
		m_renderItems.emplace_back(std::move(rightSphere));
	}

	auto skybox = std::make_unique<RenderItem>();
	skybox->m_constantBufferIndex = objCBIndex++;
	skybox->m_transform->m_scale = std::move(XMFLOAT3(5000.0f, 5000.0f, 5000.0f));
	skybox->m_topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skybox->m_material = m_material->m_data["Skybox"].get();
	skybox->m_mesh = m_meshGeos["Total"].get();
	skybox->eboCount = skybox->m_mesh->drawArgs["Sphere"].eboCount;
	skybox->eboStart = skybox->m_mesh->drawArgs["Sphere"].eboStart;
	skybox->vboStart = skybox->m_mesh->drawArgs["Sphere"].vboStart;
	m_renderItems.emplace_back(std::move(skybox));

	// 渲染不透明物体
	for (auto& item : m_renderItems)
	{
		m_renderItemLayers[static_cast<UINT>(item->m_material->type)].emplace_back(item.get());
	}
	/*
	 * 渲染镜面物体
	 */
	for (auto& item : reflectItems)
	{
		m_renderItemLayers[static_cast<UINT>(BlendType::reflective)].emplace_back(item.get());
		m_renderItems.emplace_back(std::move(item));
	}
}

void BoxApp::CreateTextures()
{
	TextureMgr::instance().Init(m_d3dDevice.Get(), m_commandQueue.Get());

	TextureMgr::instance().InsertDDSTexture("Brick", TexturePath + L"Common/bricks.dds");
	TextureMgr::instance().InsertDDSTexture("BrickNorm", TexturePath + L"Common/bricks_nmap.dds");
	TextureMgr::instance().InsertDDSTexture("Stone", TexturePath + L"Common/bricks2.dds");
	TextureMgr::instance().InsertDDSTexture("StoneNorm", TexturePath + L"Common/bricks2_nmap.dds");
	TextureMgr::instance().InsertDDSTexture("Tile", TexturePath + L"Common/checkboard.dds");
	TextureMgr::instance().InsertDDSTexture("TileNorm", TexturePath + L"Common/checkboard_nmap.dds");
	//TextureMgr::instance().InsertDDSTexture("Water", TexturePath + L"Common/water1.dds");
	//TextureMgr::instance().InsertDDSTexture("Ice", TexturePath + L"Common/ice.dds");
	m_skybox->InitStaticTex("Skybox", TexturePath + L"Skybox/grasscube1024.dds");
	m_dynamicCube->InitSRV("CubeMap");
	m_shadow->InitSRV("ShadowMap");
}

void BoxApp::CreateMaterials()
{
	m_material->CreateMaterial("Sphere", "Stone", 0, 0.3f, XMFLOAT3(0.0f, 0.0f, 0.0f), 0.3f, BlendType::opaque);
	m_material->CreateMaterial("Cube", "Brick",1, 0.1f, XMFLOAT3(0.0f, 0.0f, 0.0f), 0.1f, BlendType::opaque);
	m_material->CreateMaterial("Grid", "Tile", 2, 0.5f, XMFLOAT3(0.0f, 0.0f, 0.0f), 0.2f, BlendType::opaque);
	m_material->CreateMaterial("Skybox", m_skybox->TexName() , 3, 1.0f, XMFLOAT3(0.1f, 0.1f, 0.1f), 0.0f, BlendType::skybox);
}

auto BoxApp::CreateStaticSampler2D() -> std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7>
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
	const CD3DX12_STATIC_SAMPLER_DESC shadowSampler(6, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0.0f, 16, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, 0.0f, 8);

	return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp, shadowSampler };
}

void BoxApp::UpdateConstantBuffer(const GameTimer& timer)
{
	// 物体只需要设置一次的数据应当存储在常量缓冲区中，只有当脏标识变化时才会具体更新常量缓冲区
	auto currObjectCB = m_currFrameResource->m_uploadCBuffer.get();
	for (auto& item : m_renderItems)
	{
		item->Update(timer, currObjectCB);
	}

}

void BoxApp::UpdatePassConstant(const GameTimer& timer)
{
	// 每一帧都要随帧资源指向变化而变化的数据，如view矩阵、Projection矩阵等
	XMStoreFloat4x4(&m_currPassCB.view_gpu, XMMatrixTranspose(m_camera->GetCurrVPXM()));
	XMStoreFloat4x4(&m_currPassCB.proj_gpu, XMMatrixTranspose(m_camera->GetCurrProjXM()));
	XMStoreFloat4x4(&m_currPassCB.vp_gpu, XMMatrixTranspose(m_camera->GetCurrVPXM()));
	XMStoreFloat4x4(&m_currPassCB.shadowTransform_gpu, XMMatrixTranspose(m_shadow->GetShadowTransformXM()));
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
}

void BoxApp::UpdateOffScreen(const GameTimer& timer)
{
	m_shadow->Update(timer, [&](UINT offset, auto& constant) {
		auto passCB = m_currFrameResource->m_passCBuffer.get();
		passCB->Copy(7, constant);
	});
}

//void BoxApp::UpdateStencilFrame(const GameTimer& timer)
//{
//	auto currStencilPassCB = m_currPassCB;
//	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
//	XMMATRIX normal = XMMatrixReflect(mirrorPlane);
//
//	// 光照镜像
//	for (int i = 0; i < dirLightNum; ++i)
//	{
//		XMVECTOR lightDir = XMLoadFloat3(&m_currPassCB.lights[i].direction);
//		XMVECTOR reflectDir = XMVector3TransformNormal(lightDir, normal);
//		XMStoreFloat3(&currStencilPassCB.lights[i].direction, reflectDir);
//	}
//	auto passCB = m_currFrameResource->m_passCBuffer.get();
//	passCB->Copy(1, currStencilPassCB);
//}

void BoxApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& items)
{
	constexpr UINT objectCBByteSize = D3DUtil::AlignsConstantBuffer(sizeof(ConstantBuffer));
	auto objectConstantBuffer = m_currFrameResource->m_uploadCBuffer->GetResource();
	for (auto& item : items)
	{
		{
			const auto& vboView = item->m_mesh->GetVBOView();
			const auto& eboView = item->m_mesh->GetEBOView();
			cmdList->IASetVertexBuffers(0, 1, &vboView);
			cmdList->IASetIndexBuffer(&eboView);
			cmdList->IASetPrimitiveTopology(item->m_topologyType);
		}

		D3D12_GPU_VIRTUAL_ADDRESS CBAddress = objectConstantBuffer->GetGPUVirtualAddress();
		CBAddress += objectCBByteSize * item->m_constantBufferIndex;
		cmdList->SetGraphicsRootConstantBufferView(1, CBAddress);
		//// 为了绘制当前的帧资源和物体需要对偏移符堆做偏移
		//UINT cbvIndex = m_currFrameResourceIndex * static_cast<UINT>(items.size()) + item->m_constantBufferIndex;
		//auto cbvHandler = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
		//cbvHandler.Offset(cbvIndex, m_cbvUavDescriptorSize);

		//cmdList->SetGraphicsRootDescriptorTable(0, cbvHandler);
		cmdList->DrawIndexedInstanced(item->eboCount, 1, item->eboStart, item->vboStart, 0);
	}
}
