#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <d3dcompiler.h>
#include <vector>
#include <initializer_list>
#include <unordered_map>

using namespace std;
using Microsoft::WRL::ComPtr;

enum ShaderType
{
	default_shader = 0,
	default_with_geometry = 1,
	compute_shader = 2,
};
enum class ShaderPos : int
{
	vertex = 0,
	fragment = 1,
	geometry = 2,
	compute = 3,
	mesh = 4,
	Count
};

enum class BlendType :int {
	opaque = 0,
	transparent,
	skybox,
	alpha,
	mirror,
	reflective,
	Count
};

class Shader {
public:
	// 顶点着色器以Vert的形式，像素着色器以Frag的形式
	Shader(ShaderType type, const wstring& fileName, 
			initializer_list<D3D_SHADER_MACRO*> defines 
		,initializer_list<D3D12_INPUT_ELEMENT_DESC> args);
	Shader(ShaderType type, const wstring& binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC> args);
	const D3D12_INPUT_ELEMENT_DESC* GetInputLayouts() const;
	UINT GetInputLayoutSize() const;
	const ComPtr<ID3DBlob>& GetShaderByType(ShaderPos type) const;
private:
	/*
	 * input寄存器 ：顶点组件通过合适的定点声明映射到对应语义
	 * const寄存器和地址寄存器：存放不随顶点变化的参数
	 * output寄存器：用于存储shader计算得到的结果
	 * 临时寄存器：将数据从输入寄存器移动到临时寄存器并在临时寄存器上执行计算
	 */
	std::vector<ComPtr<ID3DBlob>>		m_shaderBlobs;
	vector<D3D12_INPUT_ELEMENT_DESC>	m_inputLayouts;
	/*
	 * HLSL没有引用、指针等概念，而是使用结构体或多个输出参数返回多个数值且HLSL的所有函数都是内联函数
	 * 通过语义概念，shader直到从哪里读取数据并把数据输送的到哪里。
	 * 常用语义参见：https://blog.csdn.net/huaixiaoniu/article/details/73551332
	 */ 
	ComPtr<ID3DBlob> CompilerShader(const wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entry, const std::string& target) const;
	void EmplaceInput(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> args);
	ComPtr<ID3DBlob> LoadBinary(const std::wstring& fileName);
};

