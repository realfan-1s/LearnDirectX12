#pragma once

#include <DirectXMath.h>
#include "D3DAPP_Template.h"
#include "FrameResource.h"
#include <memory>
#include "Shader.h"
#include "Mesh.h"
#include "Material.h"
#include <array>
#include "EffectHeader.h"

using namespace DirectX;
using namespace Template;

class BoxApp final : public D3DApp_Template<BoxApp> {
public:
	explicit BoxApp(HINSTANCE instance, bool startMsaa, Camera::CameraType type);
	BoxApp(const BoxApp&) = delete;
	BoxApp& operator=(const BoxApp&) = delete;
	bool Init();
	~BoxApp() override;
	void Resize();
	void Update(const GameTimer& timer);
	void DrawScene(const GameTimer& timer);
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnKeyboardInput(const GameTimer& timer);
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateOffScreenRendering();
	void UpdateLUT(const GameTimer& timer);
	void DrawLUT();
private:
	//void CreateConstantBuffers();
	/*
	 * ��ǩ������һϵ�и�����������ɵģ����������������Ϳ�ѡ
	 * 1.�������������������õ������������е�һ��������Χ������ȷ��Ҫ�󶨵���Դ
	 * 2.������������ͨ��ֱ�����ø�����������ָ��Ҫ�󶨵���Դ�������轫��������������У���ֻ��CBV��SRV��UAV����
	 * 3.����������������������ֱ�Ӱ�һϵ��32�ĳ���ֵ
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
	auto CreateStaticSampler2D() -> std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7>;

	void UpdateObjectInstance(const GameTimer& timer);
	void UpdatePassConstant(const GameTimer& timer); 
	void UpdateMaterialConstant(const GameTimer& timer);
	void UpdateOffScreen(const GameTimer& timer);

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& items);
	void DrawPostProcess(ID3D12GraphicsCommandList* cmdList);

	// cbuffer���������������Ա���ɫ������������,ͨ����CPUÿ֡����һ�Ρ������Ҫ�����������������ϴ��Ѷ���Ĭ�϶��У��ҳ�����������С������Ӳ����С����ռ�(256B)��������
	std::vector<std::unique_ptr<FrameResource>>			m_frame_cBuffer;
	FrameResource*										m_currFrameResource{ nullptr };
	int													m_currFrameResourceIndex{ 0 };
	std::vector<std::unique_ptr<RenderItem>>			m_renderItems;
	std::vector<RenderItem*>							m_renderItemLayers[static_cast<UINT>(BlendType::Count)];
	PassConstant										m_currPassCB;
	UINT												m_cbvOffset{ 0 };

	ComPtr<ID3D12RootSignature>							m_rootSignature{ nullptr };
	// ComPtr<ID3D12DescriptorHeap>						m_cbvHeap{ nullptr };

	unordered_map<string, std::unique_ptr<Mesh>>		m_meshGeos;
	std::shared_ptr<Material>							m_material{ nullptr };
	ComPtr<ID3D12PipelineState>							m_pso;
	std::vector<std::shared_ptr<Light>>					m_lights;

	POINT												m_lastMousePos;
	unique_ptr<Shader>									m_shader;
	std::unique_ptr<Effect::CubeMap>					m_skybox;
	std::unique_ptr<Effect::DynamicCubeMap>				m_dynamicCube;
	std::unique_ptr<Effect::Shadow>						m_shadow;

	std::unique_ptr<Effect::GaussianBlur>				m_blur;
	std::unique_ptr<Effect::ToneMap>					m_toneMap;
};   

