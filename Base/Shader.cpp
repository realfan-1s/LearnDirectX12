#include "Shader.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include "D3DUtil.hpp"

Shader::Shader
(ShaderType type, const wstring& fileName, 
			initializer_list<D3D_SHADER_MACRO*> defines 
		,initializer_list<D3D12_INPUT_ELEMENT_DESC> args)
: m_shaderBlobs(static_cast<UINT>(ShaderPos::Count))
{
	switch (type)
	{
		case default_shader:
			m_shaderBlobs[static_cast<int>(ShaderPos::vertex)] = CompilerShader(fileName, *(defines.begin()), "Vert", "VS_5_1");
			m_shaderBlobs[static_cast<int>(ShaderPos::fragment)] = CompilerShader(fileName, *(defines.begin() + 1), "Frag", "PS_5_1");
			break;
		case compute_shader:
			m_shaderBlobs[static_cast<int>(ShaderPos::compute)] = CompilerShader(fileName, *(defines.begin()), "Compute", "CS_5_1");
			break;
		case default_with_geometry:
			m_shaderBlobs[static_cast<int>(ShaderPos::vertex)] = CompilerShader(fileName, *(defines.begin()), "Vert", "VS_5_1");
			m_shaderBlobs[static_cast<int>(ShaderPos::fragment)] =  CompilerShader(fileName, *(defines.begin() + 1), "Geom", "GS_5_1");
			m_shaderBlobs[static_cast<int>(ShaderPos::fragment)] = CompilerShader(fileName, *(defines.begin() + 2), "Frag", "PS_5_1");
			break;
	}
	EmplaceInput(args);
}

Shader::Shader(ShaderType type, const wstring& binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC> args)
: m_shaderBlobs(static_cast<UINT>(ShaderPos::Count))
{
	switch (type)
	{
		case default_shader:
			m_shaderBlobs[static_cast<int>(ShaderPos::vertex)] =  LoadBinary(binaryName + L"_vs.cso");
			m_shaderBlobs[static_cast<int>(ShaderPos::fragment)] =  LoadBinary(binaryName + L"_ps.cso");
			break;
		case compute_shader:
			m_shaderBlobs[static_cast<int>(ShaderPos::compute)] =  LoadBinary(binaryName + L"_cs.cso");
			break;
		case default_with_geometry:
			m_shaderBlobs[static_cast<int>(ShaderPos::vertex)] = LoadBinary(binaryName + L"_vs.cso");
			m_shaderBlobs[static_cast<int>(ShaderPos::geometry)] = LoadBinary(binaryName + L"_gs.cso");
			m_shaderBlobs[static_cast<int>(ShaderPos::fragment)] = LoadBinary(binaryName + L"_ps.cso");
			break;
	}
	EmplaceInput(args);
}

ComPtr<ID3DBlob> Shader::CompilerShader(const wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entry, const std::string& target) const {
		UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
		compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		HRESULT res = S_OK;
		ComPtr<ID3DBlob> byteCode = nullptr;
		ComPtr<ID3DBlob> error = nullptr;
		res = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry.c_str(), target.c_str(), compileFlags, 0, &byteCode, &error);

		if(error != nullptr)
			OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
		ThrowIfFailed(res);

		return byteCode;
}

void Shader::EmplaceInput(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> args) {
	std::for_each(args.begin(), args.end(), [this](const auto& arg) {
		this->m_inputLayouts.emplace_back(arg);});
}

ComPtr<ID3DBlob> Shader::LoadBinary(const std::wstring& fileName)
{
	std::ifstream fileBinary(fileName, std::ios::binary);
	fileBinary.seekg(0, std::ios_base::end);
	std::ifstream::pos_type size = static_cast<int>(fileBinary.tellg());
	fileBinary.seekg(0, std::ios_base::beg);
	ComPtr<ID3DBlob> blob;
	D3DCreateBlob(size, blob.GetAddressOf());
	fileBinary.read(static_cast<char*>(blob->GetBufferPointer()), size);
	fileBinary.close();
	return blob;
}

const D3D12_INPUT_ELEMENT_DESC* Shader::GetInputLayouts() const
{
	return m_inputLayouts.data();
}

UINT Shader::GetInputLayoutSize() const
{
	return static_cast<UINT>(m_inputLayouts.size());
}

const ComPtr<ID3DBlob>& Shader::GetShaderByType(ShaderPos type) const
{
	return m_shaderBlobs[static_cast<int>(type)];
}
