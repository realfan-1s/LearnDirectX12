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
	// ���ʶ����ʾ������������Ѿ������ı䣬��Ҫ���ж�Ӧ�ĳ���������
	int							dirtyFlag{ frameResourcesCount };
	// ����ָ���GPU������������Ӧ�ڵ�ǰ��Ⱦ������峣��������
	UINT						m_constantBufferIndex = -1;
	Mesh*						m_mesh{ nullptr};
	MaterialData*				m_material{ nullptr };
	// DrawIndexedInstanced�ķ�������
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

	// submesh��AABB��Χ��
	DirectX::BoundingBox bound;
	virtual ~Submesh() = default;
};

/*
 * ��Щ���㻺���������������Ļ��ƣ���ֻ����Ϊ��������������Ⱦ��ˮ������׼����Ȼ��ʹ��DrawInstanced
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

	// buffers��ƫ�ơ����ȵ���Ϣ
	UINT					vbo_stride{ 0 };
	UINT					vbo_bufferByteSize{ 0 };
	DXGI_FORMAT				ebo_format{ DXGI_FORMAT_R16_UINT };
	UINT					ebo_bufferByteSize{ 0 };

	// һ��MeshGeometry�ṹ����Դ洢һ�鶥��/�����������еĶ�������壬ͨ��unordered_map���������Ե����������е�������
	unordered_map<std::string, Submesh> drawArgs;

	D3D12_VERTEX_BUFFER_VIEW GetVBOView() const;
	D3D12_INDEX_BUFFER_VIEW GetEBOView() const;
	void Dispose();
};

/*
 * ���ڴ�����Բ���Ȼ�����״
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
