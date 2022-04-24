#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "D3DUtil.hpp"
#include "GameTimer.h"
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
#include "Camera.h"

using namespace Microsoft::WRL;


class D3DApp {
public:
	D3DApp(HINSTANCE instance, bool startMSAA, Camera::CameraType type);

	D3DApp(const D3DApp&) = delete;
	D3DApp& operator=(const D3DApp&) = delete;
	D3DApp(D3DApp&& rhs) noexcept;
	D3DApp& operator=(D3DApp&& rhs) noexcept;
	virtual ~D3DApp();

	static D3DApp* GetApp();
	HINSTANCE AppInstance() const;// 获取实例应用的句柄
	HWND MainWindows() const; // 获取主窗口句柄
	float GetAspectRatio() const; // 获取荧幕宽高比

	int Run(); // 游戏主循环

	// 客户端派生类需要重载实现需求
	virtual bool Init(); // 初始化窗口和D3D
	virtual void Resize(); // 变动窗口尺寸
	virtual void Update(const GameTimer& timer) = 0; // 每一帧事件更新
	virtual void DrawScene(const GameTimer& timer) = 0; // 每一帧绘制
	virtual LRESULT MsgProc(HWND win, UINT msg, WPARAM wParam, LPARAM lParam);

	virtual void OnMouseDown(WPARAM btnState, int x, int y){ }
	virtual void OnMouseUp(WPARAM btnState, int x, int y)  { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y){ }
private:
	bool InitMainWindow(); // 初始化win32主窗口
	bool InitD3D(); // 初始化DirectX
protected:
	void CalculateFrame(); // 显示帧数
	void FlushCommandQueue();
	void CreateSwapChain();
	void CreateCommandObjects();
	void CreateRtvAndDsvDescriptorHeaps();
	void SetMSAAState(bool val);

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const;
	ID3D12Resource* GetCurrentBackBuffer() const;

	static D3DApp* m_app;

	HINSTANCE	appInstance	{ nullptr }; // 应用实例句柄
	HWND		mainWindow	{ nullptr }; // 主窗口句柄
	bool		appPaused	{ false }; // 应用暂停
	bool		minimized	{ false }; // 应用最小化
	bool		maximized	{ false }; // 应用最大化
	bool		resizeState	{ false }; // 窗口大小变化
	GameTimer	m_timer; // 计时器

	template <typename T>
	using ComPtr = ComPtr<T>;
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
	ComPtr<ID3D12DescriptorHeap>		m_rtvHeap; // 将交换链中用于渲染数据的缓冲区资源创建对应的渲染目标视图
	ComPtr<ID3D12DescriptorHeap>		m_dsvHeap; // 将深度测试的深度/模板缓冲区创建一个深度/模板视图

	std::shared_ptr<Camera>				m_camera{ nullptr };
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
