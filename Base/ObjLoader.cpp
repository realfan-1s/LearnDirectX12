#include "ObjLoader.h"
#include <iostream>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <filesystem>

#include "Scene.h"

using namespace Models;
using namespace Assimp;

const std::string_view Models::ModelPath = "Resources/Models/";

void ObjLoader::Init(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	m_device = device;
	m_cmdList = cmdList;
}

bool ObjLoader::CreateObjFromFile(std::string_view fileName)
{
	HashID id = StringToID(fileName);
	if (m_models.count(id))
		return true;
	Importer importer;
	string fullName(ModelPath);
	fullName.append(fileName);
	const aiScene* scene = importer.ReadFile(fullName, aiProcess_ConvertToLeftHanded | aiProcess_Triangulate | aiProcess_ImproveCacheLocality | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE | !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return false;
	}
	m_models[id] = std::make_shared<Model>();
	ProcessNode(scene->mRootNode, scene, fileName);
	return true;
}

bool ObjLoader::CreateObjFromFile(const char* fileName)
{
	const std::string_view files(fileName);
	return CreateObjFromFile(files);
}

optional<shared_ptr<Model>> ObjLoader::GetObj(std::string_view fileName)
{
	HashID id = StringToID(fileName);
	if (!m_models.count(id))
		return nullopt;
	return m_models[id];
}

void ObjLoader::UpdateSpecificModel
(std::string_view fileName, const GameTimer& timer,
UploaderBuffer<MaterialConstant>* currMatConstant)
{
	HashID id = StringToID(fileName);
	m_models[id]->objMat->Update(timer, currMatConstant);
}

void ObjLoader::UpdateAllModels(const GameTimer& timer, UploaderBuffer<MaterialConstant>* currMatConstant)
{
	for (const auto& [id, mod] : m_models)
	{
		mod->objMat->Update(timer, currMatConstant);
	}
}

void ObjLoader::ProcessNode(aiNode* node, const aiScene* scene, std::string_view fileName)
{
	const HashID id = StringToID(fileName);
	m_models[id]->meshData.resize(scene->mNumMeshes);
	m_models[id]->submesh.resize(scene->mNumMeshes);
	UINT vboOffset = 0;
	UINT eboOffset = 0;
	// 处理材质
	for (UINT i = 0; i < scene->mNumMaterials; ++i)
	{
		const auto& mat = scene->mMaterials[i];
		std::unique_ptr<MaterialData> data = make_unique<MaterialData>();
		data->type = BlendType::opaque;
		data->materialCBIndex = Material::GetMatIndex();
		data->name = mat->GetName().C_Str();
		data->diffuseIndex = LoadMaterialTextures(mat, aiTextureType_DIFFUSE, fileName, i);
		data->metalnessIndex = LoadMaterialTextures(mat, aiTextureType_SPECULAR, fileName, i);
		data->normalIndex = LoadMaterialTextures(mat, aiTextureType_AMBIENT, fileName, i);
		m_models[id]->objMat->CreateMaterial(std::move(data));
	}

	// 处理节点所有的网格
	for (UINT i = 0; i < scene->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[i];
		m_models[id]->submesh[i].materialName = scene->mMaterials[mesh->mMaterialIndex]->GetName().C_Str();
		m_models[id]->submesh[i].vboStart = vboOffset;
		m_models[id]->submesh[i].eboStart = eboOffset;
		m_models[id]->submesh[i].eboCount = mesh->mNumFaces * 3;
		ProcessMesh(mesh, scene, fileName, i);
		vboOffset += mesh->mNumVertices;
		eboOffset += mesh->mNumFaces * 3;
		BoundingBox box;
		box.CreateFromPoints(box, mesh->mNumVertices, reinterpret_cast<const XMFLOAT3*>(mesh->mVertices), sizeof(XMFLOAT3));
		if (i == 0)
		{
			Scene::sceneBox = std::move(box);
		} else
		{
			Scene::sceneBox.CreateMerged(Scene::sceneBox, Scene::sceneBox, box);
		}
	}
}

void ObjLoader::ProcessMesh(const aiMesh* mesh, const aiScene* scene, std::string_view fileName, UINT idx)
{
	HashID id = StringToID(fileName);
	auto& vbos = m_models[id]->meshData[idx].VBOs;
	vbos.reserve(mesh->mNumVertices);
	auto& ebos = m_models[id]->meshData[idx].EBOs;
	ebos.reserve(3LL * mesh->mNumFaces);
	// 处理顶点位置
	for (UINT i = 0; i < mesh->mNumVertices; ++i)
	{
		vbos.emplace_back(Vertex_CPU{});
		// 顶点位置、法线、坐标
		vbos[i].pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
		vbos[i].normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
		vbos[i].tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
		vbos[i].bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
		mesh->mTextureCoords[0] ? vbos[i].tex = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y } : vbos[i].tex = { 0.0f, 0.0f };
	}
	// 处理EBO
	for (UINT i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];
		for (UINT j = 0; j < face.mNumIndices; ++j)
		{
			ebos.emplace_back(face.mIndices[j]);
		}
	}
}

UINT ObjLoader::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string_view fileName, UINT idx)
{
	HashID id = StringToID(fileName);
	string fullPath(ModelPath);
	fullPath.append(fileName);
	const filesystem::path myPath(fullPath);
	aiString aiPath;
	if (mat->GetTextureCount(type) > 0)
	{
		mat->GetTexture(type, 0, &aiPath);
		filesystem::path tex = myPath.parent_path() / aiPath.C_Str();
		if (type == aiTextureType_DIFFUSE)
			return TextureMgr::instance().InsertDDSTexture(aiPath.C_Str(), tex.wstring());
		if (type == aiTextureType_SPECULAR)
			return TextureMgr::instance().InsertDDSTexture(aiPath.C_Str(), tex.wstring());
		if (type == aiTextureType_AMBIENT)
			return TextureMgr::instance().InsertDDSTexture(aiPath.C_Str(), tex.wstring());
	}
	return 0;
}

Models::ObjLoader::ObjLoader(Singleton<ObjLoader>::Token) : Singleton<Models::ObjLoader>(), m_modelMesh(std::make_unique<Mesh>())
{}