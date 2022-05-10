#pragma once
#include "Singleton.hpp"
#include <Windows.h>
#include <wrl/client.h>
#include <d3d12.h>

class RtvDsvMgr : public Singleton<RtvDsvMgr> {
public:
	explicit RtvDsvMgr(typename Singleton<RtvDsvMgr>::Token)
	: Singleton<RtvDsvMgr>(), m_rtvNumDescriptor(0), m_dsvNumDescriptor(0) {}
	~RtvDsvMgr() override = default;
	RtvDsvMgr(const RtvDsvMgr&) = delete;
	RtvDsvMgr& operator=(const RtvDsvMgr&) = delete;
	RtvDsvMgr(RtvDsvMgr&&) = delete;
	RtvDsvMgr& operator=(RtvDsvMgr&&) = delete;
	UINT RegisterRTV(UINT count)
	{
		UINT temp = m_rtvNumDescriptor;
		m_rtvNumDescriptor += count;
		return temp;
	}
	UINT RegisterDSV(UINT count)
	{
		UINT temp = m_dsvNumDescriptor;
		m_dsvNumDescriptor += count;
		return temp;
	}
	void CreateRtvAndDsvDescriptorHeaps(ID3D12Device* device)
	{
		m_rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_dsvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.NumDescriptors = m_rtvNumDescriptor;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf()));

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		dsvHeapDesc.NumDescriptors = m_dsvNumDescriptor;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;
		device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf()));
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
	{
		return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const
	{
		return m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	}
private:
	template <typename  T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	UINT							m_rtvNumDescriptor;
	UINT							m_dsvNumDescriptor;
	UINT							m_rtvSize;
	UINT							m_dsvSize;
	ComPtr<ID3D12DescriptorHeap>	m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap>	m_dsvHeap;
};

