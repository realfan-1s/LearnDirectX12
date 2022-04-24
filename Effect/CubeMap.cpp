#include "CubeMap.h"

using namespace Effect;

CubeMap::CubeMap(){
	m_shader = make_unique<Shader>(default_shader, L"Shaders\\Skybox", initializer_list<D3D12_INPUT_ELEMENT_DESC>({ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} }));
}

void CubeMap::InitStaticTex(std::string_view name, const std::wstring& fileName)
{
	TextureMgr::instance().InsertDDSTexture(name, fileName);
	staticTex = name;
}

void CubeMap::InitPSO(ID3D12Device* m_device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& templateDesc)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyboxDesc = templateDesc;
	skyboxDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// 减少深度比较，depth func耀设置为less_equal
	skyboxDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyboxDesc.InputLayout = { m_shader->GetInputLayouts(), m_shader->GetInputLayoutSize() };
	skyboxDesc.VS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::vertex)->GetBufferSize() };
	skyboxDesc.PS = { static_cast<BYTE*>(m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferPointer()), m_shader->GetShaderByType(ShaderPos::fragment)->GetBufferSize() };
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&skyboxDesc, IID_PPV_ARGS(&m_pso)));
}

const std::string_view& CubeMap::TexName() const {
	return staticTex;
}

ID3D12PipelineState* CubeMap::GetPSO() const
{
	return m_pso.Get();
}

HashID CubeMap::GetStaticID()
{
	return TextureMgr::instance().GetRegisterType(staticTex).value_or(0);
}