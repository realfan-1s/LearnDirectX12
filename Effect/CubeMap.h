#pragma once

#include "Texture.h"
#include "D3DUtil.hpp"
#include "Shader.h"

namespace Effect
{
class CubeMap {
public:
	CubeMap();
	~CubeMap() = default;
	CubeMap(const CubeMap&) = delete;
	CubeMap(CubeMap&&) = default;
	CubeMap& operator=(const CubeMap&) = delete;
	CubeMap& operator=(CubeMap&&) = default;

	void InitStaticTex(std::string_view name, const std::wstring& fileName);
	void InitPSO(ID3D12Device* m_device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc);
	const std::string_view& TexName() const;
	ID3D12PipelineState* GetPSO() const;
	HashID GetStaticID();
private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	std::string_view			staticTex;
	ComPtr<ID3D12PipelineState>	m_pso;
	std::unique_ptr<Shader>     m_shader;
};
}

