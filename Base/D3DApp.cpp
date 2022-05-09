#include "D3DApp.h"
#include <windowsx.h>
#include <vector>
#include <cassert>
#include <string>


LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp::D3DApp(HINSTANCE instance, bool startMSAA, Camera::CameraType type)
: appInstance(instance), m_clientWidth(1920), m_clientHeight(1080), m_msaaState(startMSAA) {
	if (type == Camera::first)
		m_camera = std::make_shared<FirstPersonCamera>();
	assert(m_app == nullptr);
	m_app = this;
}

D3DApp::D3DApp(D3DApp&& rhs) noexcept{
	*this = std::move(rhs);
}

D3DApp& D3DApp::operator=(D3DApp&& rhs) noexcept {
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
		m_camera = std::move(m_camera);
		m_scissorRect = rhs.m_scissorRect;
		m_mainCaption = std::move(rhs.m_mainCaption);
	}
	return *this;
}

D3DApp::~D3DApp() {
	if (m_d3dDevice != nullptr)
		FlushCommandQueue();
}

D3DApp* D3DApp::m_app = nullptr;
D3DApp* D3DApp::GetApp() {
	return m_app;
}

HINSTANCE D3DApp::AppInstance() const {
	return appInstance;
}

HWND D3DApp::MainWindows() const {
	return mainWindow;
}

float D3DApp::GetAspectRatio() const {
	return static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight);
}

int D3DApp::Run() {
	MSG msg{ nullptr };
	m_timer.Reset();
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0 ,0, PM_REMOVE))
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

bool D3DApp::Init() {
	if (!InitMainWindow())
		return false;
	if (!InitD3D())
		return false;

	Resize();
	return true;
}

void D3DApp::Resize() {
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
	m_swapChain->ResizeBuffers(m_swapBufferCount, m_clientWidth, m_clientHeight, m_backBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

	// ������ȾĿ����ͼ
	m_currBackBuffer = 0;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandler(GetCurrentBackBufferView());
	for (UINT i = 0; i < m_swapBufferCount; ++i)
	{
		m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffers[i]));
		m_d3dDevice->CreateRenderTargetView(m_swapChainBuffers[i].Get(), nullptr, rtvHeapHandler); // pDesc����Ϊ��ָ���ʾ���ø���Դ����ʱ�÷ָ�ʽ��Ϊ���ĵ�һ��MIPMAP�㼶����һ����ͼ
		rtvHeapHandler.Offset(1, m_rtvDescriptorSize);
	}

	/*
	 * �������/ģ�建������ͼ, GPU��Դ����Heap�У������Ǿ����ض����Ե�GPU�Դ��
	 * Ĭ�϶ѣ���˶����ύ����Դֻ��GPU���Է���
	 * �ϴ��ѣ��˶����ύ��CPU�ϴ���GPU����Դ
	 * �ض��ѣ���˶����ύ��CPU��ȡ����Դ
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

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = m_depthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	// ���ô���Դ�ĸ�ʽ��������0��mipmap��������
	m_d3dDevice->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, GetDepthStencilView());
	// ����Դ�ӳ�ʼ״̬װ��Ϊ��Ȼ�����
	ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE>(m_commandList.Get(), m_depthStencilBuffer.Get());
	m_commandList->Close();
	// ִ��resize����������
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue();

	m_camera->SetViewPort(0.0f, 0.0f,
		static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight),
		0.0f, 1.0f);
	m_scissorRect = { 0, 0, m_clientWidth, m_clientHeight };
}

LRESULT D3DApp::MsgProc(HWND win, UINT msg, WPARAM wParam, LPARAM lParam) {
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

bool D3DApp::InitMainWindow() {
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = MainWindowProc; 
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = appInstance;
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = L"MainWnd";

	if(!RegisterClass(&wc))
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

bool D3DApp::InitD3D() {
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
{
	ComPtr<ID3D12Debug> debugController;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
	debugController->EnableDebugLayer();
}
#endif
	CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)); // ���ڼ����ӿ�ָ�룬�������õĽӿ�ָ���Զ����IIDֵ
	HRESULT res = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_d3dDevice)); // ����d3dDeviceӲ���豸
	if (FAILED(res)) // һ��Ӳ���豸����ʧ�ܾ�Ҫ���л���
	{
		ComPtr<IDXGIAdapter> m_warpAdapter;
		m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&m_warpAdapter));
		D3D12CreateDevice(m_warpAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_d3dDevice));
	}

	m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	// �����DX11��ע�ع��ܵķ�װ��DX12����עDescriptor���ڴ��еĲ���
	m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_cbvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// ����4X MSAA���������֧��
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels;
	msaaQualityLevels.Format = m_backBufferFormat;
	msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msaaQualityLevels.SampleCount = 4;
	msaaQualityLevels.NumQualityLevels = 0;
	m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaQualityLevels, sizeof(msaaQualityLevels));
	m_msaaQuality = msaaQualityLevels.NumQualityLevels;
#if defined(DEBUG) || defined(_DEBUG)
	assert(m_msaaQuality > 0 && "Unexpected MSAA quality level.");
#endif

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DApp::CalculateFrame() {
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

/*
 * Ϊ�˱���GPUд������û�п�ʼ���Ѿ���ĳ����ԴR��˳����д�����������Դð�գ���Ҫ����Դ����Ϊ�����״̬ת��
 */
void D3DApp::FlushCommandQueue() { // ʹ��Fenceǿ����CPU�ȴ���ֱ��GPU�����������,
	m_currFence++; // ����Χ��ֵ���������ǵ���Χ����
	/*
	 * ��������������һ������Χ���������GPU������֮ǰ�޷������µ�Χ����
	 * CommandQueue->Signal��GPU������Χ���㣬Fence::Signal���Ǵ�CPU����Χ����
	*/ 
	m_commandQueue->Signal(m_fence.Get(), m_currFence);
	if (m_fence->GetCompletedValue() < m_currFence)
	{
		HANDLE eventHandler = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		// ����GPU����Χ����֮������¼�
		m_fence->SetEventOnCompletion(m_currFence, eventHandler);
		// �ȴ�ֱ��GPU����Χ����
		WaitForSingleObject(eventHandler, INFINITE);
		CloseHandle(eventHandler);
	}
}

void D3DApp::CreateSwapChain() {
	// ���ͷ�֮ǰ������˫���壬�ٽ����ؽ�
	m_swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC chainDesc;
	chainDesc.BufferDesc.Width = m_clientWidth;
	chainDesc.BufferDesc.Height = m_clientHeight;
	chainDesc.BufferDesc.RefreshRate.Numerator = 120;
	chainDesc.BufferDesc.RefreshRate.Denominator = 1;
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
	// ���������ˢ��˫���彻����
	m_dxgiFactory->CreateSwapChain(m_commandQueue.Get(), &chainDesc, m_swapChain.GetAddressOf());
}

// ÿ��gpu������ά����һ��������У������Ǳ���Ƶ�����ڴ洴��ȡ�������䡣�ڴ�һֱֻ����һ��Ļ��λ���������͸�������б��ύ����
void D3DApp::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // �洢����һϵ�пɹ�GPUֱ��ִ�е���� D3D12_COMMAND_LIST_TYPE_BUNDLE�������б���
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	m_d3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)); // ��ȡcommandQueue��ID������ǿ������ת��
	m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf()));
	m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf()));
	m_commandList->Close(); // ��Ϊ������п��ܻ���������������е����ݣ�������ȷ��GPUִ��������֮ǰ����������CommandAllocator
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = m_swapBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf()));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	m_d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf()));
}

void D3DApp::SetMSAAState(bool val) {
	if (m_msaaState != val)
	{
		m_msaaState = val;
		CreateSwapChain();
		Resize();
	}
}

void D3DApp::LogAdapters() { // ʹ��IDXGIFactory����IDXGISwapChain��ö����ʾ����������ʾ��������IDXGIAdapter��ʾ
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
	} // ����Կ�����

	for (const auto& currAdapter : adapterVec)
	{
		LogAdapterOutputs(currAdapter.Get()); // get����ʹ�ڲ������ü�����1
	}
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter) {
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

void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) const { // ��ȡ��ʾ����Դ˸�ʽ֧�ֵ�ȫ����ʾģʽ
	UINT count = 0;
	UINT flag = 0;
	output->GetDisplayModeList(format, flag, &count, nullptr); // ��ʾѡ��������ʽ
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

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::GetDepthStencilView() const {
	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::GetCurrentBackBufferView() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), 
		m_currBackBuffer,
		m_rtvDescriptorSize);
}

ID3D12Resource* D3DApp::GetCurrentBackBuffer() const {
	return m_swapChainBuffers[m_currBackBuffer].Get();
}
