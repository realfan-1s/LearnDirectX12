#pragma once

#include <d3d12.h>
#include <functional>
#include <Src/d3dx12.h>
#include <wrl/client.h>
#include "GameTimer.h"
#include "Shader.h"
#include "Vertex.h"

namespace Effect
{
class RenderToTexture {
public:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	RenderToTexture(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	virtual ~RenderToTexture() = default;
	RenderToTexture(const RenderToTexture&) = delete;
	RenderToTexture& operator=(const RenderToTexture&) = delete;
	RenderToTexture(RenderToTexture&&) = default;
	RenderToTexture& operator=(RenderToTexture&&) = default;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSRV() const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSRV() const;
	ID3D12Resource* GetResource() const;
	D3D12_VIEWPORT GetViewPort() const;
	D3D12_RECT GetScissorRect() const;
	ID3D12PipelineState* GetPSO() const;
	UINT GetSrvIdx() const;

	void OnResize(UINT newWidth, UINT newHeight);
	void InitDepthAndStencil(ID3D12GraphicsCommandList* cmdList);
	virtual void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) = 0;
	virtual void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const = 0;
	virtual void InitPSO(ID3D12Device* m_device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) = 0;
	virtual void InitShader(const std::wstring& binaryName);
	void InitSRV(std::string_view name);
	void InitDSV(CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandler);
protected:
	virtual void CreateDescriptors() = 0;
	virtual void CreateResources() = 0;
protected:
	ComPtr<ID3D12Device>			m_device;
	ComPtr<ID3D12Resource>			m_resources;
	ComPtr<ID3D12Resource>			m_depthStencilRes;
	ComPtr<ID3D12PipelineState>		m_pso;

	D3D12_VIEWPORT					m_viewport;
	D3D12_RECT						m_scissorRect;
	UINT							m_width;
	UINT							m_height;
	UINT							m_srvIndex;
	DXGI_FORMAT						m_format;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuDSV;

	std::unique_ptr<Shader>			m_shader;
};
}

