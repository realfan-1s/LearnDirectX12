#pragma once
#include <string>
#include <DirectXMath.h>
#include <d3d12.h>
#include "D3DUtil.hpp"
#include "MathHelper.hpp"
#include <unordered_map>
#include "UploaderBuffer.hpp"
#include "GameTimer.h"
#include "Shader.h"

struct MaterialData
{
	std::string			name;
	// ���ʳ���������
	UINT				materialCBIndex;
	// ����������(gAlbedo)��SRV���е�����
	UINT				diffuseIndex;
	// ������ͼ��SRV���е�����
	UINT				normalIndex;
	UINT				metalnessIndex;
	int					dirtyFlag = frameResourcesCount;
	BlendType			type;
	// ������ɫ�Ĳ��ʳ�������������
	DirectX::XMFLOAT3	emission{ 0.0f, 0.0f, 0.0f };
	float				roughness{ MathHelper::MathHelper::Rand() };
	float				metalness{ MathHelper::MathHelper::Rand() };
	MaterialData() = default;
	MaterialData(std::string _name, UINT _matIndex, float _roughness, const XMFLOAT3& _emission, float _metalness, BlendType _type);
	MaterialData(const MaterialData&) = default;
	MaterialData(MaterialData&&) = default;
	virtual ~MaterialData() = default;
};

struct MaterialConstant
{
	DirectX::XMFLOAT3	emission{ 0.0f, 0.0f, 0.0f };
	UINT				diffuseIndex{ 0 };
	UINT				normalIndex{ 0 };
	UINT				metalnessIndex{ 0 };
	XMFLOAT2			gamePad0;
};

class Material
{
public:
	static INT GetMatSize();
	static INT GetMatIndex();
	std::unordered_map<std::string, std::unique_ptr<MaterialData>> m_data;
	void CreateMaterial(const std::string& name, std::string_view texName, UINT materialCBIndex, float roughness, const XMFLOAT3& emission, float metalness, BlendType type);
	void CreateMaterial(std::unique_ptr<MaterialData> data);
	void Update(const GameTimer& timer, UploaderBuffer<MaterialConstant>* currMatConstant);
private:
	static INT matIndex;
};