#include "Mesh.h"

void RenderItem::Update(const GameTimer& timer, UploaderBuffer<ConstantBuffer>* currObjectCB)
{
	// ֻ����Щ���������˸ı�Ż���³����������е����ݣ����һ��ÿ��֮��Դ�����и���
	if (dirtyFlag > 0)
	{
		XMMATRIX model = Transform::GetModelMatrixXM(
				m_transform->m_scale, 
				m_transform->m_rotation,
				m_transform->m_position);
		ConstantBuffer objectConstant;
		XMStoreFloat4x4(&objectConstant.model_gpu, std::move(XMMatrixTranspose(model)));
		objectConstant.matIndex_gpu = m_material->materialCBIndex;
		currObjectCB->Copy(m_constantBufferIndex, objectConstant);
		dirtyFlag--;
	}
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVBOView() const
{
	/*
	* IASetVertexBuffers(UINT StartSlot, UINT NumView, const D3D12_VERTEX_BUFFER_VIEW* p_view)
	* StartSlot:�󶨶�����㻺��������ʼ�����
	* NumView:����۰󶨵Ļ���������
	* p_View:ָ�򶥵㻺������ͼ����ĵ�һ��Ԫ�ص�ָ��
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
