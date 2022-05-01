#pragma once
#include "UploaderBuffer.hpp"
#include "Vertex.h"
#include "Material.h" 

/*
 * ��CPUÿ֡����Ҫ���µ���Դ��Ϊ����Ԫ�أ������������飬��֡��Դ��
 * �ڴ����n֡ʱ��CPU�ܶ���ʼ�Ĵ�֡��Դ�����л�ȡ��һ�����õ�֡��Դ��
 * ��GPU�����ǰ֡ʱ��CPU��Ϊ��n֡������Դ���������ύ��Ӧ�������б�
 */
class FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT matCount);
	FrameResource(const FrameResource&) = delete;
	FrameResource& operator=(const FrameResource&) = delete;
	FrameResource(FrameResource&&) = default;
	FrameResource& operator=(FrameResource&&) = default;
	~FrameResource() = default;
	std::unique_ptr<UploaderBuffer<ObjectInstance>>		m_uploadCBuffer{ nullptr };
	std::unique_ptr<UploaderBuffer<PassConstant>>		m_passCBuffer{ nullptr };
	std::unique_ptr<UploaderBuffer<MaterialConstant>>	m_materialCBuffer{ nullptr };
	// GPUִ���������������ص�����֮ǰ���Ͳ�������������������ÿһ֡����Ҫ�Լ������������
	ComPtr<ID3D12CommandAllocator>						m_commandAllocator{ nullptr };
	UINT64												m_fence{ 0 };
};


