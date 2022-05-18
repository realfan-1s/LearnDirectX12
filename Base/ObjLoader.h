#pragma once

#include <d3d12.h>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <wrl/client.h>
#include "Mesh.h"
#include "Material.h"
#include "D3DUtil.hpp"
#include "Texture.h"

namespace Models
{
struct SubAdvMesh : Submesh
{
	std::string materialName;
	UINT faceStart{ 0 };
	UINT faceCount{ 0 };
};
struct AdvMeshData : BaseMeshData
{
	std::vector<XMFLOAT3> boneWeights;
	std::vector<BYTE> boneEBOs[4];
};
struct AdvMaterialData : MaterialData
{
	UINT metalRoughSRVIndex;
	UINT displacementSRVIndex;
};
struct Model
{
	vector<SubAdvMesh>			submesh;
	vector<AdvMeshData>			meshData;
	std::unique_ptr<Material>	objMat;
	Model() : objMat(std::make_unique<Material>()) {  }
};
using Microsoft::WRL::ComPtr;
extern const std::string_view ModelPath;

class ObjLoader : public Singleton<ObjLoader>{
public:
	explicit ObjLoader(typename Singleton<ObjLoader>::Token);
	ObjLoader(const ObjLoader&) = delete;
	ObjLoader(ObjLoader&&) = delete;
	ObjLoader& operator=(const ObjLoader&) = delete;
	ObjLoader& operator=(ObjLoader&&) = delete;

	void Init(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
	bool CreateObjFromFile(std::string_view fileName);
	bool CreateObjFromFile(const char* fileName);
	template <const char*... T>
	void CreateObjFromFile();
	optional<shared_ptr<Model>> GetObj(std::string_view fileName);
	void UpdateSpecificModel(std::string_view fileName, const GameTimer& timer, UploaderBuffer<MaterialConstant>* currMatConstant);
	void UpdateAllModels(const GameTimer& timer, UploaderBuffer<MaterialConstant>* currMatConstant);
	~ObjLoader() override = default;
private:
	void ProcessNode(aiNode* node, const aiScene* scene, std::string_view fileName);
	void ProcessMesh(const aiMesh* mesh, const aiScene* scene, std::string_view fileName, UINT idx);
	UINT LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string_view fileName, UINT idx);
private:
	ComPtr<ID3D12Device>							m_device;
	ComPtr<ID3D12GraphicsCommandList>				m_cmdList;
	std::unordered_map<size_t, shared_ptr<Model>>	m_models;
	std::unique_ptr<Mesh>							m_modelMesh;
};

template <const char*... T>
void ObjLoader::CreateObjFromFile()
{
	static_assert(sizeof...(T) > 0);
	(CreateObjFromFile(T), ...);
}
}
