#include "ObjLoader.h"
#include <fstream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace Models;
using namespace Assimp;

// ReSharper disable once CppVariableCanBeMadeConstexpr
const std::string_view Models::ModelPath = "Resources/Models/";

ObjLoader& ObjLoader::instance()
{
	static unique_ptr<ObjLoader> instance(new ObjLoader());
	return *instance;
}

void ObjLoader::Init(ID3D12Device* device, ID3D12CommandQueue* queue)
{
	m_device = device;
	m_commandQueue = queue;
}

const Model* ObjLoader::CreateObjFromFile(std::string_view fileName)
{
	HashID id = StringToID(fileName);
	if (m_models.count(id))
	{
		return m_models[id].get();
	}
	Importer importer;
	std::string fullPath(ModelPath);
	fullPath.append(fileName);
	auto aiScene = importer.ReadFile(fullPath, aiProcess_ConvertToLeftHanded | aiProcess_GenBoundingBoxes | aiProcess_Triangulate | aiProcess_ImproveCacheLocality | aiProcess_CalcTangentSpace);
	if (aiScene && !(aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && aiScene->HasMeshes())
	{
		std::shared_ptr<Model> model = std::make_shared<Model>();
		model->submesh.reserve(aiScene->mNumMeshes);
		model->materialData.reserve(aiScene->mNumMaterials);

		unsigned int vboCount = 0, eboCount = 0;
		for (auto i = 0U; i < aiScene->mNumMeshes; ++i)
		{
			auto aiMesh = aiScene->mMeshes[i];
			vboCount += aiMesh->mNumVertices;
			// 添加EBO
			for (auto j = 0U; j < aiMesh->mNumFaces; ++j)
			{
				aiFace face = aiMesh->mFaces[j];
				eboCount += face.mNumIndices;
			}
		}

		SubAdvMesh submesh;
		AdvMeshData meshData;
		AdvMaterialData matData;
		meshData.VBOs.reserve(vboCount);
		meshData.EBOs.reserve(eboCount);
		// process node part
		for (auto i = 0U; i < aiScene->mNumMeshes; ++i)
		{
			submesh.vboStart = meshData.VBOs.size();
			submesh.eboStart = meshData.EBOs.size();
			auto aiMesh = aiScene->mMeshes[i];
			// 添加VBO
			for (auto j = 0U; j < aiMesh->mNumVertices; ++j)
			{
				Vertex_CPU vert{};

				XMFLOAT3 pos(aiMesh->mVertices[j].x, aiMesh->mVertices[j].y, aiMesh->mVertices[j].z);
				vert.pos = std::move(pos);
				if (aiMesh->HasNormals())
				{
					XMFLOAT3 norm(aiMesh->mNormals[j].x, aiMesh->mNormals[j].y, aiMesh->mNormals[j].z);
					vert.normal = std::move(norm);
				}
				if (aiMesh->mTextureCoords[0])
				{
					XMFLOAT2 uv(aiMesh->mTextureCoords[0][j].x, aiMesh->mTextureCoords[0][j].y);
					XMFLOAT3 tan(aiMesh->mTangents[j].x, aiMesh->mTangents[j].y, aiMesh->mTangents[j].z);
					vert.tex = std::move(uv);
					vert.tangent = std::move(tan);
				}

				meshData.VBOs.emplace_back(std::move(vert));
			}

			// 添加EBO
			for (auto j = 0U; j < aiMesh->mNumFaces; ++j)
			{
				aiFace face = aiMesh->mFaces[j];
				for (auto n = 0U; n < face.mNumIndices; ++n)
				{
					meshData.EBOs.emplace_back(face.mIndices[n]);
				}
			}

			// 加载纹理
			model->submesh.emplace_back(submesh);
			model->materialData.emplace_back(matData);
		}
		model->meshData = std::move(meshData);
		m_models[id] = std::move(model);
		return model.get();
	}
	return nullptr;
}

void ObjLoader::RegisterTex(const std::wstring& fileName)
{
}

ObjLoader::ObjLoader() = default;
