#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT matCount)
: m_uploadCBuffer(std::make_unique<UploaderBuffer<ObjectInstance>>(device, objectCount, false)),
m_passCBuffer(std::make_unique<UploaderBuffer<PassConstant>>(device, passCount, true)),
m_materialCBuffer(std::make_unique<UploaderBuffer<MaterialConstant>>(device, matCount, false)),
m_postProcessCBuffer(std::make_unique<UploaderBuffer<PostProcessPass>>(device, 1, true))
{
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf()));
}

