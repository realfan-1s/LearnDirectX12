#include "Texture.h"
#include <D3DUtil.hpp>
#include <Inc/DDSTextureLoader.h>
#include <Inc/ResourceUploadBatch.h>

const std::wstring TexturePath = L"Resources/Textures/";

textureData::textureData(std::string_view _name, std::wstring _fileName)
: name(_name), fileName(std::move(_fileName))
{
	
}

Texture::Texture(std::string_view _name, std::wstring _fileName, ID3D12Device* device, ID3D12CommandQueue* queue)
	: m_tex(std::make_unique<textureData>(_name, _fileName))
{
	CreateDDSTexture(device, queue);
}

void Texture::CreateDDSTexture(ID3D12Device* currDevice, ID3D12CommandQueue* cmdQueue)
{
	DirectX::ResourceUploadBatch upload(currDevice);
	upload.Begin();
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile(currDevice, upload, m_tex->fileName.c_str(), m_tex->Resource.ReleaseAndGetAddressOf(), false, 0UI64, nullptr, &m_tex->isCubeMap));
	const auto uploadFinished = upload.End(cmdQueue);
	uploadFinished.wait();
}

TextureMgr& TextureMgr::instance()
{
	static std::unique_ptr<TextureMgr> instance(new TextureMgr());
	return *instance;
}

void TextureMgr::Init(ID3D12Device* currDevice, ID3D12CommandQueue* cmdQueue)
{
	m_device = currDevice;
	m_commandQueue = cmdQueue;
	m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

bool TextureMgr::InsertDDSTexture(std::string_view name, const std::wstring& fileName)
{
	HashID id = StringToID(name);
	if (m_textureID.count(id))
	{
		return false;
	}
	m_textures.emplace_back(std::make_unique<Texture>(name, fileName, m_device.Get(), m_commandQueue.Get()));
	m_textureID[id] = numDescriptor++;
	return true;
}

void TextureMgr::GenerateSRVHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
	srvHeapDesc.NumDescriptors = numDescriptor;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandler(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	for (size_t i = 0; i < m_textures.size(); ++i)
	{
		const auto& tex = m_textures[i];
		srvDesc.Format = tex->m_tex->Resource->GetDesc().Format;
		if (tex->m_tex->isCubeMap) {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = tex->m_tex->Resource->GetDesc().MipLevels;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		} else {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = tex->m_tex->Resource->GetDesc().MipLevels;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}
		m_device->CreateShaderResourceView(tex->m_tex->Resource.Get(), &srvDesc, srvHandler);
		srvHandler.Offset(1, m_srvDescriptorSize);
	}
}

UINT TextureMgr::RegisterRenderToTexture(std::string_view name)
{
	HashID id = StringToID(name);
	if (m_textureID.count(id))
	{
		return m_textureID[id];
	}
	m_textureID[id] = numDescriptor++;
	return m_textureID[id];
}

ID3D12DescriptorHeap* TextureMgr::GetSRVDescriptorHeap() const
{
	return m_srvHeap.Get();
}

std::optional<UINT> TextureMgr::GetRegisterType(std::string_view name)
{
	HashID id = StringToID(name);
	if (!m_textureID.count(id))
		return std::nullopt;
	return m_textureID[id];
}

size_t TextureMgr::Size() const
{
	return m_textureID.size();
}

TextureMgr::~TextureMgr() = default;
