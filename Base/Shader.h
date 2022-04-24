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
	// ������ɫ����Vert����ʽ��������ɫ����Frag����ʽ
	Shader(ShaderType type, const wstring& fileName, 
			initializer_list<D3D_SHADER_MACRO*> defines 
		,initializer_list<D3D12_INPUT_ELEMENT_DESC> args);
	Shader(ShaderType type, const wstring& binaryName, initializer_list<D3D12_INPUT_ELEMENT_DESC> args);
	const D3D12_INPUT_ELEMENT_DESC* GetInputLayouts() const;
	UINT GetInputLayoutSize() const;
	const ComPtr<ID3DBlob>& GetShaderByType(ShaderPos type) const;
private:
	/*
	 * input�Ĵ��� ���������ͨ�����ʵĶ�������ӳ�䵽��Ӧ����
	 * const�Ĵ����͵�ַ�Ĵ�������Ų��涥��仯�Ĳ���
	 * output�Ĵ��������ڴ洢shader����õ��Ľ��
	 * ��ʱ�Ĵ����������ݴ�����Ĵ����ƶ�����ʱ�Ĵ���������ʱ�Ĵ�����ִ�м���
	 */
	std::vector<ComPtr<ID3DBlob>>		m_shaderBlobs;
	vector<D3D12_INPUT_ELEMENT_DESC>	m_inputLayouts;
	/*
	 * HLSLû�����á�ָ��ȸ������ʹ�ýṹ���������������ض����ֵ��HLSL�����к���������������
	 * ͨ��������shaderֱ���������ȡ���ݲ����������͵ĵ����
	 * ��������μ���https://blog.csdn.net/huaixiaoniu/article/details/73551332
	 */ 
	ComPtr<ID3DBlob> CompilerShader(const wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entry, const std::string& target) const;
	void EmplaceInput(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> args);
	ComPtr<ID3DBlob> LoadBinary(const std::wstring& fileName);
};

