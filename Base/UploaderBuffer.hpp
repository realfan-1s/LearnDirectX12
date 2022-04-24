#pragma once

#include <D3DUtil.hpp>

using Microsoft::WRL::ComPtr;

template <typename T>
class UploaderBuffer {
public:
	UploaderBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : m_isConstantBuffer(isConstantBuffer)
	{
		if (isConstantBuffer)
		{
			m_elementByteSize = D3DUtil::AlignsConstantBuffer(sizeof(T));
		} else
		{
			m_elementByteSize = sizeof(T);
		}
		{
			const auto& property = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			const auto& buffer = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(m_elementByteSize) * elementCount);
			ThrowIfFailed(device->CreateCommittedResource(
				&property,
				D3D12_HEAP_FLAG_NONE,
				&buffer,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_uploadBuffer)));
		}
		// 在使用完资源之前，无需取消映射但是需要同步
		m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_data));
	}
	~UploaderBuffer()
	{
		if (m_uploadBuffer != nullptr)
		{
			m_uploadBuffer->Unmap(0, nullptr);
		}
		m_data = nullptr;
	}
	UploaderBuffer(const UploaderBuffer&) = delete;
	UploaderBuffer& operator=(const UploaderBuffer&) = delete;
	UploaderBuffer(UploaderBuffer&& rhs) noexcept
	{
		*this = std::move(rhs);
	}
	UploaderBuffer& operator=(UploaderBuffer&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_uploadBuffer = std::move(rhs.m_uploadBuffer);
			if (!rhs.m_data)
			{
				m_data = rhs.m_data;
				rhs.m_data = nullptr;
			}
			m_elementByteSize = rhs.m_elementByteSize;
			rhs.m_elementByteSize = 0;
			m_isConstantBuffer = rhs.m_isConstantBuffer;
			rhs.m_isConstantBuffer = false;
		}
		return *this;
	}
	void Copy(int elementIndex, const T& data)
	{
		memcpy(&m_data[elementIndex * m_elementByteSize], &data, sizeof(T));
	}
	ID3D12Resource* GetResource() const
	{
		return m_uploadBuffer.Get();
	}
private:
	ComPtr<ID3D12Resource>	m_uploadBuffer;
	BYTE*					m_data{ nullptr };
	UINT					m_elementByteSize{ 0 };
	bool					m_isConstantBuffer{ false };
};

