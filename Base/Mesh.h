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

class Mesh;
using namespace std;
using Microsoft::WRL::ComPtr;

struct RenderItem
{
	RenderItem() : m_transform(make_shared<Transform>()) {}
	std::shared_ptr<Transform>	m_transform{ nullptr };
	D3D12_PRIMITIVE_TOPOLOGY	m_topologyType{ D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
	// 脏标识来表示物体相关数据已经发生改变，需要更行对应的常量缓冲区
	int							dirtyFlag{ frameResourcesCount };
	// 索引指向的GPU常量缓冲区对应于当前渲染项的物体常量缓冲区
	UINT						m_constantBufferIndex = -1;
	Mesh*						m_mesh{ nullptr};
	MaterialData*				m_material{ nullptr };
	// DrawIndexedInstanced的方法参数
	UINT						eboCount{ 0 };
	UINT						eboStart{ 0 };
	int							vboStart{ 0 };
	void Update(const GameTimer& timer, UploaderBuffer<ConstantBuffer>* currObjectCB);
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
