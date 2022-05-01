#pragma once
#include "UploaderBuffer.hpp"
#include "Vertex.h"
#include "Material.h" 

/*
 * 以CPU每帧都需要更新的资源作为基本元素，创建环形数组，即帧资源。
 * 在处理第n帧时，CPU周而复始的从帧资源数组中获取下一个可用的帧资源。
 * 趁GPU处理此前帧时，CPU将为第n帧更新资源并构建和提交对应的命令列表
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
	// GPU执行完命令分配器相关的命令之前，就不能重置命令分配器因此每一帧都需要自己的命令分配器
	ComPtr<ID3D12CommandAllocator>						m_commandAllocator{ nullptr };
	UINT64												m_fence{ 0 };
};


