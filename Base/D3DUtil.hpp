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
	 * 为了方便GPU访问顶点数组，将顶点数组放置在顶点缓冲区中，缓冲区不支持多维资源、mipmap、多重采样等
	 * width表示缓冲区所占用的字节数
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
				IID_PPV_ARGS(ans.GetAddressOf())); // 创建实际的默认缓冲区资源
		}
		// 创建默认缓冲区
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
		// 描述希望复制到默认缓冲区的数据
		D3D12_SUBRESOURCE_DATA subresource_data {initData, byteSize, byteSize};
		/*
		 * 将数据复制到默认缓冲区
		 * UpdateSubresource会先将数据从CPU端的内存复制到中介位置的上传堆
		 * 再通过调用ID3D12CommandList::CopySubresourceRegion函数复制到m_buffer中
		 */
		ChangeState<D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST>(cmdList, ans.Get());
		UpdateSubresources<1>(cmdList, ans.Get(), uploadBuffer.Get(), 0, 0, 1, &subresource_data); // 命令列表；目标资源；中间资源；中间资源偏移；中间资源起始点；资源中的子资源数
		ChangeState<D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ>(cmdList, ans.Get());
		return ans;
	}
	// 若常量缓冲区中存储的是随不同物体而异的常量数据，绘制n个物体就需要好n个常量缓冲区,需要将常量缓冲区大小对齐256B
	// CPU需要自行手动对齐，但GPU不需要
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