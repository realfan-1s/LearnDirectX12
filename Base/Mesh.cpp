#include "Mesh.h"

UINT Item::GetInstanceCount()
{
	return instanceCount;
}

void RenderItem::Update(const GameTimer& timer, UploaderBuffer<ObjectInstance>* currObjectCB, UINT offset)
{
	instanceStart = offset;
	// 只有这些变量发生了改变才会更新常量缓冲区中的数据，而且会对每个之资源都进行更新
	for (UINT i = 0; i < transformPack.size(); ++i)
	{
		XMMATRIX model = Transform::GetModelMatrixXM(
			transformPack[i]->m_scale,
			transformPack[i]->m_rotation,
			transformPack[i]->m_position);
		ObjectInstance instanceData;
		XMStoreFloat4x4(&instanceData.model_gpu, std::move(XMMatrixTranspose(model)));
		instanceData.matIndex_gpu = m_matIndex;
		currObjectCB->Copy(i + offset, instanceData);
	}
}

UINT RenderItem::GetInstanceSize() const {
	return static_cast<UINT>(transformPack.size());
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVBOView() const
{
	/*
	* IASetVertexBuffers(UINT StartSlot, UINT NumView, const D3D12_VERTEX_BUFFER_VIEW* p_view)
	* StartSlot:绑定多个顶点缓冲区的起始输入槽
	* NumView:输入槽绑定的缓冲区数量
	* p_View:指向顶点缓冲区视图数组的第一个元素的指针
	*/
	const D3D12_VERTEX_BUFFER_VIEW vbo{ vbo_gpu->GetGPUVirtualAddress(), vbo_bufferByteSize, vbo_stride };
	return vbo;
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetEBOView() const
{
	const D3D12_INDEX_BUFFER_VIEW ebo{ ebo_gpu->GetGPUVirtualAddress(), ebo_bufferByteSize, ebo_format };
	return ebo;
}

void Mesh::Dispose()
{
	vbo_uploader = nullptr;
	ebo_uploader = nullptr;
}

std::vector<uint16_t>& BaseMeshData::GetEBO_16()
{
	if (EBOs_16.empty())
	{
		EBOs_16.reserve(EBOs.size());
		for (auto i = 0; i < EBOs.size(); ++i)
		{
			EBOs_16.emplace_back(static_cast<uint16_t>(EBOs[i]));
		}
	}
	return EBOs_16;
}
