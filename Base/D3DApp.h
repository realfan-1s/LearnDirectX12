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
	HINSTANCE AppInstance() const;// ��ȡʵ��Ӧ�õľ��
	HWND MainWindows() const; // ��ȡ�����ھ��
	float GetAspectRatio() const; // ��ȡӫĻ��߱�

	int Run(); // ��Ϸ��ѭ��

	// �ͻ�����������Ҫ����ʵ������
	virtual bool Init(); // ��ʼ�����ں�D3D
	virtual void Resize(); // �䶯���ڳߴ�
	virtual void Update(const GameTimer& timer) = 0; // ÿһ֡�¼�����
	virtual void DrawScene(const GameTimer& timer) = 0; // ÿһ֡����
	virtual LRESULT MsgProc(HWND win, UINT msg, WPARAM wParam, LPARAM lParam);

	virtual void OnMouseDown(WPARAM btnState, int x, int y){ }
	virtual void OnMouseUp(WPARAM btnState, int x, int y)  { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y){ }
private:
	bool InitMainWindow(); // ��ʼ��win32������
	bool InitD3D(); // ��ʼ��DirectX
protected:
	void CalculateFrame(); // ��ʾ֡��
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

	HINSTANCE	appInstance	{ nullptr }; // Ӧ��ʵ�����
	HWND		mainWindow	{ nullptr }; // �����ھ��
	bool		appPaused	{ false }; // Ӧ����ͣ
	bool		minimized	{ false }; // Ӧ����С��
	bool		maximized	{ false }; // Ӧ�����
	bool		resizeState	{ false }; // ���ڴ�С�仯
	GameTimer	m_timer; // ��ʱ��

	template <typename T>
	using ComPtr = ComPtr<T>;
	ComPtr<ID3D12Device>				m_d3dDevice;
	ComPtr<IDXGIFactory6>				m_dxgiFactory;// ID3D11DeviceContext��D3D12�кϲ�ΪIDXGIFactory6
	ComPtr<IDXGISwapChain>				m_swapChain; // DX˫����
	ComPtr<ID3D12Fence>					m_fence;
	UINT64								m_currFence{ 0 };
	ComPtr<ID3D12CommandQueue>			m_commandQueue; // ֻҪ���յ�����commandList��ָ���������ʼִ��
	ComPtr<ID3D12CommandAllocator>		m_commandAllocator; // ÿ���̶̹߳�һ��allocator ÿ���߳̿�¼�ƶ��commandList��������ύ
	ComPtr<ID3D12GraphicsCommandList>	m_commandList; // ���ύ����¼��������һ���ύ��commandQueue
	/*
	 * RTV����ʾ��Դ��ֻд��RENDER TARGET�� SRV����ʾ��Դ��ֻ����TEXTURE�� CBV����ʾ��Դ��ֻ����BUFFER��
	 * DSV����ʾ��Դ��ֻд��DEPTH STENCIL�� UAV����ʾ��Դ���������д
	 */
	UINT								m_rtvDescriptorSize		{ 0 };
	UINT								m_dsvDescriptorSize		{ 0 };
	UINT								m_cbvUavDescriptorSize	{ 0 };

	static constexpr int				m_swapBufferCount{ 2 };
	int									m_currBackBuffer{ 0 }; // ָ���ض��ĺ�̨����������
	ComPtr<ID3D12Resource>				m_swapChainBuffers[m_swapBufferCount]; // Ϊÿһ��������������һ��RenderTarget(����)
	ComPtr<ID3D12Resource>				m_depthStencilBuffer; // �����ģ������RenderTarget

	// ��ΪResource��gpu�д�����ڴ�飬�޷�����⣬�����Ҫ����ı�������ʹ��resource
	ComPtr<ID3D12DescriptorHeap>		m_rtvHeap; // ����������������Ⱦ���ݵĻ�������Դ������Ӧ����ȾĿ����ͼ
	ComPtr<ID3D12DescriptorHeap>		m_dsvHeap; // ����Ȳ��Ե����/ģ�建��������һ�����/ģ����ͼ

	std::shared_ptr<Camera>				m_camera{ nullptr };
	D3D12_RECT							m_scissorRect; // �ü�����,����֮������ݶ��ᱻ�޳�
	std::wstring						m_mainCaption { L"D3DX12 APP" };
	int									m_clientWidth;
	int									m_clientHeight;

	/*
	* 4x MSAA��ز���
	*/
	bool								m_msaaState			{ false };
	UINT								m_msaaQuality		{ 0 };
	D3D_DRIVER_TYPE						m_d3dDriverType		{ D3D_DRIVER_TYPE_HARDWARE };
	DXGI_FORMAT							m_backBufferFormat	{ DXGI_FORMAT_R8G8B8A8_UNORM };
	DXGI_FORMAT							m_depthStencilFormat{ DXGI_FORMAT_D24_UNORM_S8_UINT };
};
