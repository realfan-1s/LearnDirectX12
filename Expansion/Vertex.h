#pragma once

#include <DirectXMath.h>
#include "MathHelper.hpp"
#include "D3DUtil.hpp"
#include "Light.h"

using namespace DirectX;

struct Vertex_GPU
{
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
	XMFLOAT2 uv;
	Vertex_GPU(const XMFLOAT3& _pos, const XMFLOAT3& _normal, const XMFLOAT3& _tan, const XMFLOAT2& _uv) : pos(_pos), normal(_normal), tangent(_tan), uv(_uv) {}
	Vertex_GPU(float px, float py, float pz, float nx, float ny, float nz, float tanx, float tany, float tanz, float u, float v):
	pos(px, py, pz), normal(nx, ny, nz), tangent(tanx, tany, tanz), uv(u, v) {}
	Vertex_GPU() = default;
};

struct Vertex_CPU
{
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
	XMFLOAT3 bitangent;
	XMFLOAT2 tex;
	Vertex_CPU(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, float tz, float u, float v)
	: pos(px, py, pz), normal(nx, ny, nz), tangent(tx, ty, tz), tex(u, v) {}
	Vertex_CPU(const XMFLOAT3& _pos, const XMFLOAT3& _normal, const XMFLOAT3& _tangent, const XMFLOAT2& _tex) :
	pos(_pos), normal(_normal), tangent(_tangent), tex(_tex) {}
	Vertex_CPU() = default;
};

struct ObjectInstance
{
	XMFLOAT4X4	model_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	texTransform_gpu{ MathHelper::MathHelper::identity4x4() };
	UINT		matIndex_gpu{ 0 };
	UINT		objPad0_gpu;
	UINT		objPad1_gpu;
	UINT		objPad2_gpu;
	ObjectInstance() = default;
};

struct PassConstant
{
	XMFLOAT4X4	view_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	proj_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	vp_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	invProj_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4  shadowTransform_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	viewPortRay_gpu{ MathHelper::MathHelper::identity4x4() };
	float		nearZ_gpu{ 1.0f };
	float		farZ_gpu{ 0.0f };
	float		deltaTime_gpu{ 0.0f };
	float		totalTime_gpu{ 0.0f };
	XMFLOAT2	renderTargetSize_gpu;
	XMFLOAT2	invRenderTargetSize_gpu;
	XMFLOAT3	cameraPos_gpu;
	float		jitterX;
	XMFLOAT3	ambient{ 0.05f, 0.05f, 0.05f };
	float		jitterY;
	LightData	lights[maxLights];
};

struct PostProcessPass {
	XMFLOAT4X4	view_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	proj_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	vp_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4  nonjitteredVP_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	previousVP_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	invProj_gpu{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4	viewPortRay_gpu{ MathHelper::MathHelper::identity4x4() };
	float		nearZ_gpu{ 1.0f };
	float		farZ_gpu{ 0.0f };
	float		deltaTime_gpu{ 0.0f };
	float		totalTime_gpu{ 0.0f };
	XMFLOAT3	cameraPos_gpu;
	float		g_GamePad0;
};
