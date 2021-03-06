#pragma once

#include <DirectXMath.h>
#include <memory>
#include <array>
#include "D3DAPP_Template.h"
#include "FrameResource.h"
#include "Shader.h"
#include "Mesh.h"
#include "Material.h"
#include "EffectHeader.h"

using namespace DirectX;
using namespace Template;

class BoxApp final : public D3DApp_Template<BoxApp> {
public:
	explicit BoxApp(HINSTANCE instance, bool startMsaa, Camera::CameraType type);
	BoxApp(const BoxApp&) = delete;
	BoxApp& operator=(const BoxApp&) = delete;
	BoxApp(BoxApp&&) = default;
	BoxApp& operator=(BoxApp&&) = default;
	~BoxApp() override;
	bool Init();
	void Resize();
	void Update(const GameTimer& timer);
	void DrawScene(const GameTimer& timer);
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnKeyboardInput(const GameTimer& timer);
	void RegisterRTVAndDSV();
	void CreateOffScreenRendering();
	void UpdateLUT(const GameTimer& timer);
	void DrawLUT();
private:
	//void CreateConstantBuffers();
	/*
	 * 根签名是由一系列根参数定义而成的，根参数有三种类型可选
	 * 1.描述符表：描述符表引用的是描述符堆中的一块连续范围，用于确定要绑定的资源
	 * 2.内联描述符：通过直接设置根描述符即可指定要绑定的资源而且无需将其存入描述符堆中，但只有CBV、SRV、UAV可用
	 * 3.根常量：借助根常量可以直接绑定一系列32的常量值
	 */
	void CreateRootSignature();
	void CreateLights();
	void CreateDescriptorHeaps();
	void CreateShadersAndInput();
	void CreateGeometry();
	void CreatePSO();
	void CreateFrameResources();
	void CreateRenderItems();
	void CreateTextures();
	void CreateMaterials();
	auto CreateStaticSampler2D() -> std::array<const CD3DX12_STATIC_SAMPLER_DESC, 8>;

	void UpdateObjectInstance(const GameTimer& timer);
	void UpdatePassConstant(const GameTimer& timer); 
	void UpdateMaterialConstant(const GameTimer& timer);
	void UpdateOffScreen(const GameTimer& timer);
	void UpdatePostProcess(const GameTimer& timer);
	void UpdateLightPos(const GameTimer& timer);

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& items);
	void DrawPostProcess(ID3D12GraphicsCommandList* cmdList);
	void DrawDebugItems(ID3D12GraphicsCommandList* cmdList) const;
private:
	// cbuffer常量缓冲区，可以被着色器程序所引用,通常是CPU每帧更新一次。因此需要将常量缓冲区存在上传堆而非默认堆中，且常量缓冲区大小必须是硬件最小分配空间(256B)的整数倍
	ComPtr<ID3D12RootSignature>							m_rootSignature{ nullptr };
	FrameResource*										m_currFrameResource{ nullptr };
	std::vector<std::unique_ptr<FrameResource>>			m_frameCBuffer;
	std::vector<std::unique_ptr<RenderItem>>			m_renderItems;
	std::vector<RenderItem*>							m_renderItemLayers[static_cast<UINT>(BlendType::Count)];
	PassConstant										m_currPassCB;
	int													m_currFrameResourceIndex{ 0 };
	UINT												m_passOffset;

	unordered_map<string, std::unique_ptr<Mesh>>		m_meshGeos;
	std::shared_ptr<Material>							m_material{ nullptr };
	std::vector<std::shared_ptr<Light<Pixel>>>			m_pixelLights;
	std::vector<std::shared_ptr<Light<Compute>>>		m_computeLights;
	std::vector<unique_ptr<LightMoveParams>>			m_pointLightPos;

	POINT												m_lastMousePos;
	std::unique_ptr<Effect::CubeMap>					m_skybox;
	std::unique_ptr<Effect::DynamicCubeMap>				m_dynamicCube;
	std::unique_ptr<Effect::CascadedShadow>				m_shadow;

	std::unique_ptr<Renderer::TileBasedDefer>			m_renderer;
	std::unique_ptr<Renderer::GBuffer>					gBuffer;
	std::unique_ptr<Effect::SSAO>						m_ssao;

	std::unique_ptr<Effect::GaussianBlur>				m_blur;
	std::unique_ptr<Effect::ToneMap>					m_toneMap;
	std::unique_ptr<Effect::TemporalAA>					m_TemporalAA;
};   

