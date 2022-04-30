#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "Mesh.h"
#include "Material.h"
#include "D3DUtil.hpp"
#include "Texture.h"

namespace Models
{
	struct SubAdvMesh : Submesh
	{
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
		HashID diffuseName;
		HashID normalName;
		HashID metalRoughName;
		HashID displacementName;
	};
	struct Model
	{
		vector<SubAdvMesh>		submesh;
		AdvMeshData				meshData;
		vector<AdvMaterialData> materialData;
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

		void Init(ID3D12Device* device, ID3D12CommandQueue* queue);
		const Model* CreateObjFromFile(std::string_view fileName);
		~ObjLoader() override = default;
	private:
		ComPtr<ID3D12Device>							m_device;
		ComPtr<ID3D12CommandQueue>						m_commandQueue;
		std::unordered_map<size_t, shared_ptr<Model>>	m_models;
		std::unordered_map<size_t, unique_ptr<Texture>> m_textures;
		void RegisterTex(const std::wstring& fileName);
	};
}
