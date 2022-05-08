#pragma once

#include <string>
#include <wrl/client.h>
#include <d3d12.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include "Singleton.hpp"

using Microsoft::WRL::ComPtr;
extern const std::wstring TexturePath;

struct textureData
{
	std::string_view		name;
	std::wstring			fileName;
	bool					isCubeMap{ false };
	ComPtr<ID3D12Resource>	Resource{ nullptr };
	textureData(std::string_view _name, std::wstring _fileName);
};

class Texture {
public:
	Texture(std::string_view name, std::wstring fileName, ID3D12Device* device, ID3D12CommandQueue* queue);
	~Texture() = default;
	std::unique_ptr<textureData> m_tex;
private:
	void CreateDDSTexture(ID3D12Device* currDevice, ID3D12CommandQueue* cmdQueue);
};

class TextureMgr : public Singleton<TextureMgr>
{
public:
	TextureMgr(const TextureMgr&) = delete;
	TextureMgr(TextureMgr&&) = delete;
	TextureMgr& operator=(const TextureMgr&) = delete;
	TextureMgr& operator=(TextureMgr&&) = delete;
	void Init(ID3D12Device* __restrict currDevice, ID3D12CommandQueue* __restrict cmdQueue);
	bool InsertDDSTexture(std::string_view name, const std::wstring& fileName);
	void GenerateSRVHeap();
	UINT RegisterRenderToTexture(std::string_view name);
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const;
	std::optional<UINT> GetRegisterType(std::string_view name);
	size_t Size() const;
	virtual ~TextureMgr();
	explicit TextureMgr(typename Singleton<TextureMgr>::Token);
private:
	ComPtr<ID3D12Device>									m_device;
	ComPtr<ID3D12CommandQueue>								m_commandQueue;
	ComPtr<ID3D12DescriptorHeap>							m_srvHeap{ nullptr };
	UINT													m_srvDescriptorSize;
	std::vector<std::unique_ptr<Texture>>					m_textures;
	std::unordered_map<size_t, UINT>						m_textureID;
	UINT													numDescriptor;
};