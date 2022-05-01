#pragma once

#include <string>
#include <wrl/client.h>
#include <Src/d3dx12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXCollision.h>
#include <unordered_map>
#include "D3DUtil.hpp"
#include "Material.h"
#include "Vertex.h"
#include "Transform.h"
#include <iostream>

class Mesh;
using namespace std;
using Microsoft::WRL::ComPtr;

struct RenderItem
{
private:
	inline static UINT instanceCount = 0;
public:
	RenderItem() = default;
	std::vector<std::shared_ptr<Transform>>	transformPack;
	D3D12_PRIMITIVE_TOPOLOGY				m_topologyType{ D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
	// 脏标识来表示物体相关数据已经发生改变，需要更行对应的常量缓冲区
	// 索引指向的GPU常量缓冲区对应于当前渲染项的物体常量缓冲区
	Mesh*									m_mesh{ nullptr};
	UINT									m_matIndex;
	BlendType								m_type;
	// DrawIndexedInstanced的方法参数
	UINT									instanceStart{ 0 };
	UINT									eboCount{ 0 };
	UINT									eboStart{ 0 };
	int										vboStart{ 0 };
	void Update(const GameTimer& timer, UploaderBuffer<ObjectInstance>* currObjectCB, UINT offset);
	template <typename... Args, std::enable_if_t<sizeof...(Args) <= 3 && (is_same_v<decltype(Transform::m_scale), Args>, ...)>* = nullptr>
	void EmplaceBack(Args&&... args)
	{
		instanceCount++;
		auto& form = transformPack.emplace_back(std::make_shared<Transform>());
		auto list = std::make_tuple(std::forward<Args>(args)...);
		if constexpr (sizeof...(args) == 1)
		{
			form->m_position = std::get<0>(list);
		} else if constexpr (sizeof...(args) == 2)
		{
			form->m_position = std::get<0>(list);
			form->m_scale = std::get<1>(list);
		} else if constexpr (sizeof...(args) == 3)
		{
			form->m_position = std::get<0>(list);
			form->m_scale = std::get<1>(list);
			form->m_rotation = std::get<2>(list);
		}
	}
	void EmplaceBack()
	{
		instanceCount++;
		transformPack.emplace_back(std::make_shared<Transform>());
	}
	template <typename... Args, std::enable_if_t<sizeof...(Args) <= 3 && (is_same_v<decltype(Transform::m_scale), Args>, ...)>* = nullptr>
	void SetParameters(UINT pos, Args&&... args)
	{
		const auto& form = transformPack[pos];
		auto list = std::make_tuple(std::forward<Args>(args)...);
		if constexpr (sizeof...(args) == 1)
		{
			form->m_position = std::get<0>(list);
		} else if constexpr (sizeof...(args) == 2)
		{
			form->m_position = std::get<0>(list);
			form->m_scale = std::get<1>(list);
		} else if constexpr (sizeof...(args) == 3)
		{
			form->m_position = std::get<0>(list);
			form->m_scale = std::get<1>(list);
			form->m_rotation = std::get<2>(list);
		}
	}
	UINT GetInstanceSize() const;
	static UINT GetInstanceCount();
};

struct Submesh
{
	UINT eboCount = 0;
	UINT eboStart = 0;
	UINT vboStart = 0;

	// submesh的AABB包围盒
	DirectX::BoundingBox bound;
	virtual ~Submesh() = default;
};

/*
 * 这些顶点缓冲区并不会真正的绘制，而只是作为顶点数据送至渲染流水线做好准备，然后使用DrawInstanced
 */
class Mesh {
public:
	string name;
	ComPtr<ID3DBlob>		vbo_cpu{ nullptr };
	ComPtr<ID3DBlob>		ebo_cpu{ nullptr };
	ComPtr<ID3D12Resource>	vbo_gpu{ nullptr };
	ComPtr<ID3D12Resource>	ebo_gpu{ nullptr };
	ComPtr<ID3D12Resource>	vbo_uploader{ nullptr };
	ComPtr<ID3D12Resource>	ebo_uploader{ nullptr };

	// buffers的偏移、长度等信息
	UINT					vbo_stride{ 0 };
	UINT					vbo_bufferByteSize{ 0 };
	DXGI_FORMAT				ebo_format{ DXGI_FORMAT_R16_UINT };
	UINT					ebo_bufferByteSize{ 0 };

	// 一个MeshGeometry结构体可以存储一组顶点/索引缓冲区中的多个集合体，通过unordered_map容器，可以单独绘制其中的子网格
	unordered_map<std::string, Submesh> drawArgs;

	D3D12_VERTEX_BUFFER_VIEW GetVBOView() const;
	D3D12_INDEX_BUFFER_VIEW GetEBOView() const;
	void Dispose();
};

/*
 * 用于创建球、圆柱等基础形状
 */
struct BaseMeshData
{
	std::vector<Vertex_CPU>	VBOs;
	std::vector<uint32_t>	EBOs;

	std::vector<uint16_t>& GetEBO_16();
	virtual ~BaseMeshData() = default;
private:
	std::vector<uint16_t>	EBOs_16;
};
