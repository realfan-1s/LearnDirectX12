#pragma once

#include <comdef.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl/client.h>
#include <Src/d3dx12.h>
#include <variant>

using Microsoft::WRL::ComPtr;

inline constexpr int frameResourcesCount = 3;
inline constexpr int maxLights = 16;
inline constexpr int dirLightNum = 3;
inline constexpr int spotLightNum = 0;
inline constexpr int pointLightNum = 0;

template <D3D12_RESOURCE_STATES TBefore, D3D12_RESOURCE_STATES TAfter>
inline void ChangeState(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* res);

class D3DUtil {
public:
	/*
	 * Ϊ�˷���GPU���ʶ������飬��������������ڶ��㻺�����У���������֧�ֶ�ά��Դ��mipmap�����ز�����
	 * width��ʾ��������ռ�õ��ֽ���
	 */
	static ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		INT64 byteSize,
		ComPtr<ID3D12Resource>& uploadBuffer)
	{
		ComPtr<ID3D12Resource> ans;
		{
			const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			const auto& desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
			device->CreateCommittedResource(
				&properties, 
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COMMON, 
				nullptr, 
				IID_PPV_ARGS(ans.GetAddressOf())); // ����ʵ�ʵ�Ĭ�ϻ�������Դ
		}
		// ����Ĭ�ϻ�����
		{
			const auto& properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			const auto& desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
			device->CreateCommittedResource(
				&properties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(uploadBuffer.GetAddressOf()));
		}
		// ����ϣ�����Ƶ�Ĭ�ϻ�����������
		D3D12_SUBRESOURCE_DATA subresource_data {initData, byteSize, byteSize};
		/*
		 * �����ݸ��Ƶ�Ĭ�ϻ�����
		 * UpdateSubresource���Ƚ����ݴ�CPU�˵��ڴ渴�Ƶ��н�λ�õ��ϴ���
		 * ��ͨ������ID3D12CommandList::CopySubresourceRegion�������Ƶ�m_buffer��
		 */
		ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, ans.Get());
		UpdateSubresources<1>(cmdList, ans.Get(), uploadBuffer.Get(), 0, 0, 1, &subresource_data); // �����б�Ŀ����Դ���м���Դ���м���Դƫ�ƣ��м���Դ��ʼ�㣻��Դ�е�����Դ��
		ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, ans.Get());
		return ans;
	}
	// �������������д洢�����治ͬ�������ĳ������ݣ�����n���������Ҫ��n������������,��Ҫ��������������С����256B
	// CPU��Ҫ�����ֶ����룬��GPU����Ҫ
	static constexpr UINT AlignsConstantBuffer(UINT byteSize)
	{
		return (byteSize + 255U) & (~255U);
	}
};

class DxException
{
public:
    DxException() = default;
	DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber)
	: ErrorCode(hr), FunctionName(functionName), Filename(filename), LineNumber(lineNumber)	{}

	std::wstring ToString() const {
		_com_error error(ErrorCode);
		std::wstring msg = error.ErrorMessage();
		return FunctionName + L" Failed in " + Filename + L"; Line " + std::to_wstring(LineNumber) + L"; error: " + msg;
	}

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

inline void ReleaseCom(IDXGIAdapter* x)
{
	if (x)
	{
		x->Release();
		x = nullptr;
	}
}

inline void ReleaseCom(IDXGIOutput* x)
{
	if (x)
	{
		x->Release();
		x = nullptr;
	}
}

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

inline auto MakeBoolVariant(bool x) -> std::variant<std::true_type, std::false_type>
{
	if (x)
		return std::true_type{};
	return std::false_type{};
}

using HashID = size_t;
inline HashID StringToID(std::string_view str)
{
	static std::hash<std::string_view> hash;
	return hash(str);
}

inline HashID StringToID(const std::string& str)
{
	static std::hash<std::string> hash;
	return hash(str);
}

inline HashID StringToID(std::wstring_view str)
{
	static std::hash<std::wstring_view> hash;
	return hash(str);
}

inline HashID StringToID(const std::wstring& str)
{
	static std::hash<std::wstring> hash;
	return hash(str);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)\
{                       \
	HRESULT hr = (x);   \
	std::wstring wfn = AnsiToWString(__FILE__); \
	if(FAILED(hr)) {throw DxException(hr, L#x, wfn, __LINE__);}\
}
#endif

template <D3D12_RESOURCE_STATES TBefore, D3D12_RESOURCE_STATES TAfter>
inline void ChangeState(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* res)
{
	const auto& trans = CD3DX12_RESOURCE_BARRIER::Transition(res, TBefore, TAfter);
	cmdList->ResourceBarrier(1, &trans);
}