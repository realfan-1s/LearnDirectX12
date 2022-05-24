#pragma once

#include <d3d12.h>
#include <functional>
#include <optional>
#include <Src/d3dx12.h>
#include <wrl/client.h>
#include "GameTimer.h"
#include "Shader.h"
#include "Vertex.h"

namespace Effect
{
class RenderToTexture {
public:
	RenderToTexture(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	virtual ~RenderToTexture() = default;
	RenderToTexture(const RenderToTexture&) = delete;
	RenderToTexture& operator=(const RenderToTexture&) = delete;
	RenderToTexture(RenderToTexture&&) = default;
	RenderToTexture& operator=(RenderToTexture&&) = default;
	const CD3DX12_CPU_DESCRIPTOR_HANDLE& GetCpuSRV() const;
	const CD3DX12_GPU_DESCRIPTOR_HANDLE& GetGpuSRV() const;
	ID3D12Resource* GetResource() const;
	const D3D12_VIEWPORT& GetViewPort() const;
	const D3D12_RECT& GetScissorRect() const;
	ID3D12PipelineState* GetPSO() const;

	virtual void OnResize(UINT newWidth, UINT newHeight);
	virtual void Update(const GameTimer& timer, const std::function<void(UINT, PassConstant&)>& updateFunc) = 0;
	virtual void Draw(ID3D12GraphicsCommandList* cmdList, const std::function<void(UINT)>& drawFunc) const = 0;
	virtual void InitPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc) = 0;
	void InitSRV(std::string_view name);
	std::optional<UINT> GetSrvIdx(std::string_view name);
protected:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	virtual void CreateDescriptors() = 0;
	virtual void CreateResources() = 0;
protected:
	ComPtr<ID3D12Device>			m_device;
	ComPtr<ID3D12Resource>			m_resource;
	ComPtr<ID3D12PipelineState>		m_pso;

	CD3DX12_CPU_DESCRIPTOR_HANDLE	m_cpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE	m_gpuSRV;
	D3D12_VIEWPORT					m_viewport;
	D3D12_RECT						m_scissorRect;
	DXGI_FORMAT						m_format;
	UINT							m_width;
	UINT							m_height;
	bool							m_dirtyFlag{ false };
};
}

