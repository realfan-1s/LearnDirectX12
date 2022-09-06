#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "D3DUtil.hpp"
#include "GameTimer.h"
#include "Camera.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <string>
#include <wrl/client.h>
#include <Src/d3dx12.h>
#include <windowsx.h>
#include <vector>
#include <cassert>
#include <string>
#include "RtvDsvMgr.h"

using namespace Microsoft::WRL;

namespace Template
{
template <typename T>
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

template <typename T>
class D3DApp_Template {
public:
	explicit D3DApp_Template(HINSTANCE instance, bool startMSAA, Camera::CameraType type)
	: appInstance(instance), m_clientWidth(1920), m_clientHeight(1080), m_msaaState(startMSAA) {
		if (type == Camera::first)
			m_camera = std::make_shared<FirstPersonCamera>();

		assert(m_app == nullptr);
		m_app = this;
	}

	D3DApp_Template(const D3DApp_Template&) = delete;
	D3DApp_Template& operator=(const D3DApp_Template&) = delete;
	D3DApp_Template(D3DApp_Template&& rhs) noexcept{
		*this = std::move(rhs);
	}
	D3DApp_Template& operator=(D3DApp_Template&& rhs) noexcept{
		if (this != &rhs)
		{
			minimized = rhs.minimized;
			rhs.minimized = false;
			maximized = rhs.maximized;
			rhs.maximized = false;
			resizeState = rhs.resizeState;
			rhs.resizeState = false;
			m_currFence = rhs.m_currFence;
			rhs.m_currFence = 0;
			m_clientWidth = rhs.m_clientWidth;
			rhs.m_clientWidth = 0;
			m_clientHeight = rhs.m_clientHeight;
			rhs.m_clientHeight = 0;
			m_msaaState = rhs.m_msaaState;
			rhs.m_msaaState = false;
			m_msaaQuality = rhs.m_msaaQuality;
			rhs.m_msaaQuality = 0;
			m_d3dDriverType = rhs.m_d3dDriverType;
			rhs.m_d3dDriverType = D3D_DRIVER_TYPE_UNKNOWN;
			m_backBufferFormat = rhs.m_backBufferFormat;
			rhs.m_backBufferFormat = DXGI_FORMAT_UNKNOWN;
			m_depthStencilFormat = rhs.m_depthStencilFormat;
			rhs.m_depthStencilFormat = DXGI_FORMAT_UNKNOWN;
			if (rhs.appInstance != nullptr)
			{
				appInstance = rhs.appInstance;
				rhs.appInstance = nullptr;
			}
			if (rhs.mainWindow)
			{
				mainWindow = rhs.mainWindow;
				rhs.mainWindow = nullptr;
			}
			m_timer = rhs.m_timer;
			m_d3dDevice = std::move(rhs.m_d3dDevice);
			m_dxgiFactory = std::move(rhs.m_dxgiFactory);
			m_swapChain = std::move(rhs.m_swapChain);
			m_fence = std::move(rhs.m_fence);
			m_commandQueue = std::move(rhs.m_commandQueue);
			m_commandAllocator = std::move(rhs.m_commandAllocator);
			m_commandList = std::move(rhs.m_commandList);
			m_camera = std::move(rhs.m_camera);
			m_scissorRect = rhs.m_scissorRect;
			m_mainCaption = std::move(rhs.m_mainCaption);
		}
		return *this;
	}
	virtual ~D3DApp_Template()
	{
		if (m_d3dDevice != nullptr)
			FlushCommandQueue();
	}

	static D3DApp_Template* GetApp();
	HINSTANCE AppInstance() const // 获取实例应用的句柄
	{
		return appInstance;
	}
	HWND MainWindows() const // 获取主窗口句柄
	{
		return mainWindow;
	}
	float GetAspectRatio() const // 获取荧幕宽高比
	{
		return static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight);
	}

	int Run() // 游戏主循环
	{
		MSG msg{ nullptr };
		m_timer.Reset();
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, 0, 0 ,0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} else
			{
				m_timer.Tick();
				if (!appPaused)
				{
					CalculateFrame();
					Update(m_timer);
					DrawScene(m_timer);
				} else
					Sleep(100);
			}
		}
		return static_cast<int>(msg.wParam);
	}
public:
	bool Init()
	{
		return static_cast<T*>(this)->Init();
	}
	void Resize()
	{
		static_cast<T*>(this)->Resize();
	}
	void Update(const GameTimer& timer) // 每一帧事件更新
	{
		static_cast<T*>(this)->Update(timer);
	}
	void DrawScene(const GameTimer& timer) // 每一帧绘制
	{
		static_cast<T*>(this)->DrawScene(timer);
	}
	LRESULT MsgProc(HWND win, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
			case WM_ACTIVATE:
				if(LOWORD(wParam) == WA_INACTIVE )
				{
					appPaused = true;
					m_timer.Stop();
				}
				else
				{
					appPaused = false;
					m_timer.Start();
				}
				return 0;
			case WM_SIZE:
				m_clientWidth = LOWORD(lParam);
				m_clientHeight = HIWORD(lParam);
				if (m_d3dDevice)
				{
					if(wParam == SIZE_MINIMIZED)
					{
						appPaused = true;
						minimized = true;
						maximized = false;
					} else if (wParam == SIZE_MAXIMIZED)
					{
						appPaused = false;
						minimized = false;
						maximized = true;
						Resize();
					} else if (wParam == SIZE_RESTORED)
					{
						if(minimized)
						{
							appPaused = false;
							minimized = false;
							Resize();
						} else if (maximized)
						{
							appPaused = false;
							maximized = false;
							Resize();
						} else if (resizeState)
						{
							
						} else
							Resize();
					}
				}
				return 0;
			case WM_ENTERSIZEMOVE:
				appPaused = true;
				resizeState = true;
				m_timer.Stop();
				return 0;
			case WM_EXITSIZEMOVE:
				appPaused = false;
				resizeState = false;
				m_timer.Start();
				Resize();
				return 0;
			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			case WM_MENUCHAR:
				return MAKELRESULT(0, MNC_CLOSE);
			case WM_GETMINMAXINFO:
				((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
				((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200; 
				return 0;
			case WM_LBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_RBUTTONDOWN:
				OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				return 0;
			case WM_LBUTTONUP:
			case WM_MBUTTONUP:
			case WM_RBUTTONUP:
				OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				return 0;
			case WM_MOUSEMOVE:
				OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				return 0;
			case WM_KEYUP:
			    if(wParam == VK_ESCAPE)
			    {
			        PostQuitMessage(0);
			    }
			    else if((int)wParam == VK_F2)
			        SetMSAAState(!m_msaaState);
				return 0;
		}
		return DefWindowProc(win, msg, wParam, lParam);
	}

	void OnMouseDown(WPARAM btnState, int x, int y)
	{
		static_cast<T*>(this)->OnMouseDown(btnState, x, y);
	}
	void OnMouseUp(WPARAM btnState, int x, int y)
	{
		static_cast<T*>(this)->OnMouseUp(btnState, x, y);
	}
	void OnMouseMove(WPARAM btnState, int x, int y)
	{
		static_cast<T*>(this)->OnMouseMove(btnState, x, y);
	}
protected:
	// 客户端派生类需要重载实现需求
	bool Init(const T&) // 初始化窗口和D3D
	{
		if (!InitMainWindow())
			return false;
		if (!InitD3D())
			return false;

		Resize();
		return true;
	}
	void Resize(const T&) // 变动窗口尺寸
	{
#if defined(DEBUG) || defined(_DEBUG)
		assert(m_d3dDevice);
		assert(m_swapChain);
		assert(m_commandList);
		assert(m_commandAllocator);
#endif
		FlushCommandQueue();
		m_commandList->Reset(m_commandAllocator.Get(), nullptr);
#pragma omp parallel for
		for (int i = 0; i < m_swapBufferCount; ++i)
		{
			m_swapChainBuffers[i].Reset();
		}
		m_depthStencilBuffer.Reset();
		m_swapChain->ResizeBuffers(m_swapBufferCount, m_clientWidth, m_clientHeight, m_backBufferFormat,	DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

		// 创建渲染目标视图
		m_currBackBuffer = 0;
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandler(GetCurrentBackBufferView());
		for (UINT i = 0; i < m_swapBufferCount; ++i)
		{
			m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffers[i]));
			m_d3dDevice->CreateRenderTargetView(m_swapChainBuffers[i].Get(), nullptr, rtvHeapHandler); // pDesc设置为空指针表示采用该资源创建时得分格式并为它的第一个MIPMAP层级创建一个视图
			rtvHeapHandler.Offset(1, m_rtvDescriptorSize);
		}

		/*
		 * 创建深度/模板缓冲区视图, GPU资源存在Heap中，本质是具有特定属性的GPU显存块
		 * 默认堆：向此堆中提交的资源只有GPU可以访问
		 * 上传堆：此堆中提交经CPU上传至GPU的资源
		 * 回读堆：向此堆中提交由CPU读取的资源
		 */
		D3D12_RESOURCE_DESC dsDesc;
		dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsDesc.Alignment = 0;
		dsDesc.Width = m_clientWidth;
		dsDesc.Height = m_clientHeight;
		dsDesc.DepthOrArraySize = 1;
		dsDesc.MipLevels = 1;
		dsDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		dsDesc.SampleDesc.Count = m_msaaState ? 4 : 1;
		dsDesc.SampleDesc.Quality = m_msaaState ? (m_msaaQuality - 1) : 0;
		dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = m_depthStencilFormat;
		optClear.DepthStencil.Depth = 0.0f;
		optClear.DepthStencil.Stencil = 0;
		const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		m_d3dDevice->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&dsDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf()));
		m_depthStencilBuffer->SetName(L"currDepthStencilFrame");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = m_depthStencilFormat;
		dsvDesc.Texture2D.MipSlice = 0;
		// 利用此资源的格式，创建第0层mipmap的描述符
		m_d3dDevice->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, GetDepthStencilView());
		// 将资源从初始状态装变为深度缓冲区
		ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE>(m_commandList.Get(), m_depthStencilBuffer.Get());
		m_commandList->Close();
		// 执行resize后的命令队列
		ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
		FlushCommandQueue();

		m_camera->SetViewPort(0.0f, 0.0f,
			static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight),
			0.0f, 1.0f);
		m_scissorRect = { 0, 0, m_clientWidth, m_clientHeight };
		m_camera->SetFrustumReverseZ(XM_PI / 3, GetAspectRatio(), 0.5f, 500.0f);
	}
	bool InitMainWindow() // 初始化win32主窗口
	{
		WNDCLASS wc;
		wc.style         = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = MainWindowProc<T>; 
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = appInstance;
		wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
		wc.hCursor       = LoadCursor(0, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
		wc.lpszMenuName  = 0;
		wc.lpszClassName = L"MainWnd";

		if( !RegisterClass(&wc) )
		{
			MessageBox(0, L"RegisterClass Failed.", 0, 0);
			return false;
		}

		// Compute window rectangle dimensions based on requested client area dimensions.
		RECT R = { 0, 0, m_clientWidth, m_clientHeight };
		AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
		int width  = R.right - R.left;
		int height = R.bottom - R.top;

		mainWindow = CreateWindow(L"MainWnd", m_mainCaption.c_str(), 
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, appInstance, nullptr); 
		if(!mainWindow)
		{
			MessageBox(nullptr, L"CreateWindow Failed.", nullptr, 0);
			return false;
		}

		ShowWindow(mainWindow, SW_SHOW);
		UpdateWindow(mainWindow);

		return true;
	}
	bool InitD3D() // 初始化DirectX
	{
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
		{
			ComPtr<ID3D12Debug> debugController;
			D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
			debugController->EnableDebugLayer();
		}
#endif
		CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)); // 用于检索接口指针，根据所用的接口指针自动提过IID值
		HRESULT res = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_d3dDevice)); // 创建d3dDevice硬件设备
		if (FAILED(res)) // 一旦硬件设备创建失败就要进行回退
		{
			ComPtr<IDXGIAdapter> m_warpAdapter;
			m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&m_warpAdapter));
			D3D12CreateDevice(m_warpAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_d3dDevice));
		}

		m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
		// 相较于DX11更注重功能的封装，DX12更关注Descriptor在内存中的布局
		m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		m_cbvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// 检测对4X MSAA质量级别的支持
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels;
		msaaQualityLevels.Format = m_backBufferFormat;
		msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		msaaQualityLevels.SampleCount = 4;
		msaaQualityLevels.NumQualityLevels = 0;
		m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaQualityLevels, sizeof(msaaQualityLevels));
		m_msaaQuality = msaaQualityLevels.NumQualityLevels;
#if defined(DEBUG) || defined(_DEBUG)
		assert(m_msaaQuality > 0 && "Unexpected MSAA quality level.");
		ComPtr<ID3D12InfoQueue>	infoQueue;
		if (SUCCEEDED(m_d3dDevice.As(&infoQueue)))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
			D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
			D3D12_MESSAGE_ID denyIDs[] = {
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
			};
			D3D12_INFO_QUEUE_FILTER newFilter{};
			newFilter.DenyList.NumSeverities = _countof(severities);
			newFilter.DenyList.pSeverityList = severities;
			newFilter.DenyList.NumIDs = _countof(denyIDs);
			newFilter.DenyList.pIDList = denyIDs;
 
			ThrowIfFailed(infoQueue->PushStorageFilter(&newFilter));
		}
#endif

		CreateCommandObjects();
		CreateSwapChain();
		RegisterRTVAndDSV();
		CreateOffScreenRendering();
		CreateRtvAndDsvDescriptorHeaps();

		return true;
	}
	void CalculateFrame() const // 显示帧数
	{
		static long long frame = 0;
		static double timeElapsed = 0.0;
		frame++;

		if ((m_timer.TotalTime() - timeElapsed) >= 1.0f)
		{
			float fps = static_cast<float>(frame);
			float mspf = 1000.0f / fps;

			std::wstring fpsStr = std::to_wstring(fps);
			std::wstring mspfStr = std::to_wstring(mspf);
			std::wstring windowText = m_mainCaption +
		        L"    fps: " + fpsStr +
		        L"   mspf: " + mspfStr;

		    SetWindowText(mainWindow, windowText.c_str());
			
			// Reset for next average.
			frame = 0;
			timeElapsed += 1.0;
		}
	}
	void FlushCommandQueue()
	{
		m_currFence++; // 增加围栏值，将命令标记到此围栏点
		/*
		 * 向命令队列中添加一条新增围栏的命令，在GPU处理完之前无法设置新的围栏点
		 * CommandQueue->Signal从GPU端设置围栏点，Fence::Signal则是从CPU设置围栏点
		*/ 
		m_commandQueue->Signal(m_fence.Get(), m_currFence);
		if (m_fence->GetCompletedValue() < m_currFence)
		{
			HANDLE eventHandler = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

			// 设置GPU到达围栏点之后处理的事件
			m_fence->SetEventOnCompletion(m_currFence, eventHandler);
			// 等待直到GPU到达围栏点
			WaitForSingleObject(eventHandler, INFINITE);
			CloseHandle(eventHandler);
		}

	}
	void CreateSwapChain()
	{
		// 先释放之前创建的双缓冲，再进行重建
		m_swapChain.Reset();

		DXGI_SWAP_CHAIN_DESC chainDesc;
		chainDesc.BufferDesc.Width = m_clientWidth;
		chainDesc.BufferDesc.Height = m_clientHeight;
		chainDesc.BufferDesc.RefreshRate.Numerator = 0;
		chainDesc.BufferDesc.RefreshRate.Denominator = 0;
		chainDesc.BufferDesc.Format = m_backBufferFormat;
		chainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		chainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		chainDesc.SampleDesc.Count = m_msaaState ? 4 : 1;
		chainDesc.SampleDesc.Quality = m_msaaState ? (m_msaaQuality - 1) : 0;
		chainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		chainDesc.BufferCount = m_swapBufferCount;
		chainDesc.OutputWindow = mainWindow;
		chainDesc.Windowed = true;
		chainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		chainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		// 用命令队列刷新双缓冲交换链
		m_dxgiFactory->CreateSwapChain(m_commandQueue.Get(), &chainDesc, m_swapChain.GetAddressOf());
	}
	void CreateCommandObjects()
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 存储的是一系列可供GPU直接执行的命令， D3D12_COMMAND_LIST_TYPE_BUNDLE将命令列表打包
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		m_d3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)); // 获取commandQueue的ID并进行强制类型转换
		m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf()));
		m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf()));
		m_commandList->Close(); // 因为命令队列可能会引用命令分配其中的数据，所以在确定GPU执行完命令之前，不能重置CommandAllocator
	}
	void CreateRtvAndDsvDescriptorHeaps()
	{
		RtvDsvMgr::instance().CreateRtvAndDsvDescriptorHeaps(m_d3dDevice.Get());
	}

	void CreateOffScreenRendering()
	{
		static_cast<T*>(this)->CreateOffScreenRendering();
	}
	void SetMSAAState(bool val)
	{
		if (m_msaaState != val)
		{
			m_msaaState = val;
			CreateSwapChain();
			Resize();
		}
	}
	void RegisterRTVAndDSV()
	{
		static_cast<T*>(this)->RegisterRTVAndDSV();
	}
	void RegisterRTVAndDSV(const T&)
	{
		RtvDsvMgr::instance().RegisterRTV(m_swapBufferCount);
		RtvDsvMgr::instance().RegisterDSV(1);
	}
	void LogAdapters()
	{
		UINT i = 0;
		ComPtr<IDXGIAdapter> adapter{ nullptr };
		std::vector<ComPtr<IDXGIAdapter>> adapterVec;
		while (m_dxgiFactory->EnumAdapters(i, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);

			std::wstring text = L"-------------Adapter: ";
			text += desc.Description;
			text += L"----------------\n";

			OutputDebugString(text.c_str());
			adapterVec.emplace_back(adapter);
			++i;
		} // 输出显卡类型

		for (const auto& currAdapter : adapterVec)
		{
			LogAdapterOutputs(currAdapter.Get()); // get不会使内部的引用计数加1
		}
	}
	void LogAdapterOutputs(IDXGIAdapter* adapter)
	{
		UINT i = 0;
		ComPtr<IDXGIOutput> output{ nullptr };
		while (adapter->EnumOutputs(i, output.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_OUTPUT_DESC desc;
			output->GetDesc(&desc);
			std::wstring text = L"-------------output: ";
			text += desc.DeviceName;
			text += L"-------------\n";
			OutputDebugString(text.c_str());
			LogOutputDisplayModes(output.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
			++i;
		}
	}
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) const
	{
		UINT count = 0;
		UINT flag = 0;
		output->GetDisplayModeList(format, flag, &count, nullptr); // 表示选择的纹理格式
		std::vector<DXGI_MODE_DESC> modeVec(count);
		output->GetDisplayModeList(format, flag, &count, &modeVec[0]);

		for (const auto& x : modeVec)
		{
			const UINT n = x.RefreshRate.Numerator;
			const UINT d = x.RefreshRate.Denominator;
			std::wstring text =
		        L"Width = " + std::to_wstring(x.Width) + L" " +
		        L"Height = " + std::to_wstring(x.Height) + L" " +
		        L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
		        L"\n";
			OutputDebugString(text.c_str());
		}
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
	{
		return RtvDsvMgr::instance().GetDepthStencilView();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(RtvDsvMgr::instance().GetRenderTargetView(), 
		m_currBackBuffer,
		m_rtvDescriptorSize);
	}
	ID3D12Resource* GetCurrentBackBuffer() const
	{
		return m_swapChainBuffers[m_currBackBuffer].Get();
	}

	static D3DApp_Template* m_app;

	HINSTANCE	appInstance	{ nullptr }; // 应用实例句柄
	HWND		mainWindow	{ nullptr }; // 主窗口句柄
	bool		appPaused	{ false }; // 应用暂停
	bool		minimized	{ false }; // 应用最小化
	bool		maximized	{ false }; // 应用最大化
	bool		resizeState	{ false }; // 窗口大小变化
	GameTimer	m_timer; // 计时器

	ComPtr<ID3D12Device>				m_d3dDevice;
	ComPtr<IDXGIFactory6>				m_dxgiFactory;// ID3D11DeviceContext在D3D12中合并为IDXGIFactory6
	ComPtr<IDXGISwapChain>				m_swapChain; // DX双缓冲
	ComPtr<ID3D12Fence>					m_fence;
	UINT64								m_currFence{ 0 };
	ComPtr<ID3D12CommandQueue>			m_commandQueue; // 只要接收到来自commandList的指令就立即开始执行
	ComPtr<ID3D12CommandAllocator>		m_commandAllocator; // 每个线程固定一个allocator 每个线程可录制多个commandList，随后按需提交
	ComPtr<ID3D12GraphicsCommandList>	m_commandList; // 将提交至零录制下来并一次提交给commandQueue
	/*
	 * RTV：表示资源是只写的RENDER TARGET， SRV：表示资源是只读的TEXTURE， CBV：表示资源是只读的BUFFER，
	 * DSV：表示资源是只写的DEPTH STENCIL， UAV：表示资源可以随机读写
	 */
	UINT								m_rtvDescriptorSize		{ 0 };
	UINT								m_dsvDescriptorSize		{ 0 };
	UINT								m_cbvUavDescriptorSize	{ 0 };

	static constexpr int				m_swapBufferCount{ 2 };
	int									m_currBackBuffer{ 0 }; // 指定特定的后台缓冲区索引
	ComPtr<ID3D12Resource>				m_swapChainBuffers[m_swapBufferCount]; // 为每一个缓冲区都创建一个RenderTarget(画布)
	ComPtr<ID3D12Resource>				m_depthStencilBuffer; // 深度与模板区的RenderTarget

	// 因为Resource是gpu中纯粹的内存块，无法被理解，因此需要额外的表述帮助使用resource

	std::shared_ptr<Camera>				m_camera;
	D3D12_RECT							m_scissorRect; // 裁剪矩形,除此之外的内容都会被剔除
	std::wstring						m_mainCaption { L"D3DX12 APP" };
	int									m_clientWidth;
	int									m_clientHeight;

	/*
	* 4x MSAA相关参数
	*/
	bool								m_msaaState			{ false };
	UINT								m_msaaQuality		{ 0 };
	D3D_DRIVER_TYPE						m_d3dDriverType		{ D3D_DRIVER_TYPE_HARDWARE };
	DXGI_FORMAT							m_backBufferFormat	{ DXGI_FORMAT_R8G8B8A8_UNORM };
	DXGI_FORMAT							m_depthStencilFormat{ DXGI_FORMAT_D24_UNORM_S8_UINT };
};
template <typename T>
D3DApp_Template<T>* D3DApp_Template<T>::m_app = nullptr;
template <typename T>
D3DApp_Template<T>* D3DApp_Template<T>::GetApp()
{
	return m_app;
}

template <typename T>
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
    return D3DApp_Template<T>::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}
} 
