#ifndef TILE_BASED_CULLING
#define TILE_BASED_CULLING

#define TILE_GROUP_DIM (16)
#define TILE_GROUP_SIZE (TILE_GROUP_DIM * TILE_GROUP_DIM)
#define MAX_LIGHTS_NUM (2048)

#include "../BRDF/BRDF.hlsl"
#include "ComputeStruct.hlsl"

ConstantBuffer<ComputeConstant> cbPass		: register(b0);
cbuffer 						cbDebug 	: register(b1) {
	uint g_visualizeLightCount;
	uint g_visualizePerSampleShading;
	uint g_gamepad0;
	uint g_gamepad1;
}
StructuredBuffer<PointLight>	sbLights 	: register(t0);
Texture2D 						gBuffer[3]	: register(t1);
RWTexture2D<float4> 			output[2]   : register(u1);

SamplerState            pointWrap        : register(s0);
SamplerState            pointClamp       : register(s1);
SamplerState            linearWrap       : register(s2);
SamplerState            linearClamp      : register(s3);
SamplerState            anisotropicWrap  : register(s4);
SamplerState            anisotropicClamp : register(s5);

groupshared uint tileLightCount;
groupshared uint tileLightList[MAX_LIGHTS_NUM >> 3];
groupshared uint2 depthNearFar;

/*
* 如果给出的矩阵是投影矩阵，则裁剪面是在观察空间
* 如果给出的矩阵是VP矩阵，则裁剪面是在世界空间
* 如果给出的矩阵是MVP矩阵，则裁剪面是在物体空间
*/
void ConstructFrustumPlanes(uint2 groupID, float tileMinZ, float tileMaxZ, uint2 texSize, float4x4 mat, out float4 frustumPlane[6]);
float3 ComputePointLight(PointLight light, MaterialData mat, float3 pos, float3 normalDir, float3 viewDir);

/*
* 为了提出光源，考虑将单个视锥体基于屏幕区域划分为多个块，一个块对应子视锥体，每个分块的大小时16x16，对每个子视锥体进行一次全局光源的视锥体剔除，
* 从而分别获得各自受影响的光源列表，并在着色时根据当前像素所属的分块区域对对应的光源列表进行着色计算。
* 同时遍历该区块的所有深度值并算出对应的Zmin、Zmax作为视锥体的nearP和farP,从而缩小视锥体的大小并有效剔除光源
*/
[numthreads(TILE_GROUP_DIM, TILE_GROUP_DIM, 1)]
void Defer(uint3 groupID : SV_GROUPID, uint3 dispathID : SV_DispatchThreadID, uint groupIdx : SV_GROUPINDEX) {
	uint width, height;
	gBuffer[0].GetDimensions(width, height);
	float2 invSize = float2(1.0f / width, 1.0f / height);
	float2 globalUV = dispathID.xy * invSize;
	GBufferData data = DecodeGBuffer(gBuffer, anisotropicClamp, dispathID.xy, invSize, cbPass.g_proj, cbPass.g_view);
	MaterialData mat;
	mat.albedo = data.albedo;
	mat.roughness = data.roughness;
	mat.emission = float3(0, 0, 0);
	mat.metalness = data.metalness;

	float Zmin = max(cbPass.g_nearZ, cbPass.g_farZ);
	float Zmax = min(cbPass.g_nearZ, cbPass.g_farZ);
	// 避免对天空盒或其他非法像素着色
	bool valid = (data.viewPos.z >= min(cbPass.g_nearZ, cbPass.g_farZ) && data.viewPos.z < max(cbPass.g_nearZ, cbPass.g_farZ));
	if (valid){
		Zmin = min(data.viewPos.z, Zmin);
		Zmax = max(data.viewPos.z, Zmax);
	}
	if (groupIdx == 0) {
		// 初始化需要的部分
		tileLightCount = 0;
		depthNearFar.x = 0x7F7FFFFF;
		depthNearFar.y = 0;
	}
	GroupMemoryBarrierWithGroupSync();
	/*
	* 共享内存的压力实际上减小了内核的总体运行速度
	*/
	if (Zmax >= Zmin){
		InterlockedMin(depthNearFar.x, asuint(Zmin));
		InterlockedMax(depthNearFar.y, asuint(Zmax));
	}
	GroupMemoryBarrierWithGroupSync();

	uint2 texSize;
	gBuffer[0].GetDimensions(texSize.x, texSize.y);
	float tileMinZ = asfloat(depthNearFar.x);
	float tileMaxZ = asfloat(depthNearFar.y);
	float4 frustumPlanes[6];
	ConstructFrustumPlanes(groupID.xy, tileMinZ, tileMaxZ, texSize, cbPass.g_proj, frustumPlanes); // 变换到了View空间

	// 光源剔除,同时每个线程还要承担一部分的光源碰撞检测计算
	for (uint idx = groupIdx; idx < MAX_LIGHTS_NUM; idx += TILE_GROUP_SIZE) {
		PointLight light = sbLights[idx];
		bool inFrustum = true;
		[unroll]
		for (uint i = 0; i < 6; ++i) {
			// TODO:计算有问题!
			float dist = dot(frustumPlanes[i], float4(light.posV, 1.0f));
			inFrustum = inFrustum && (dist >= -light.fallOfEnd);
		}
		[branch]
		if (inFrustum) {
			// 会受到光照的light需要被加入列表
			uint pivot = 0;
			InterlockedAdd(tileLightCount, 1, pivot);
			tileLightList[pivot] = idx;
		}
	}
	GroupMemoryBarrierWithGroupSync();

	// 光照计算阶段,只需要处理屏幕区域内的像素即可
	if (all(dispathID.xy < texSize)) {
		[branch]
		if (g_visualizeLightCount) {
			output[0][dispathID.xy] = float4(float(tileLightCount / 255.0f).xxx, 1.0f);
			output[1][dispathID.xy] = float4(0, 0, 0, 0);
		} else {
			float3 viewDir = normalize(-data.viewPos);
			for (uint i = 0; i < tileLightCount; ++i) {
				float3 col = ComputePointLight(sbLights[tileLightList[i]], mat, data.viewPos, data.normalDir, viewDir);
				output[0][dispathID.xy] += float4(col, 1.0f);
				float3 luma = output[0][dispathID.xy].xyz;
				if (Luminance(luma) > 1.0f){
					output[1][dispathID.xy] += float4(col, 1.0f);
				}
			}
		}
	}
}

/*
Pperspective = Ppersp->ortho*Portho
Ppersp->ortho = 				Portho =
  {n,  0, 	0, 		0,				   {1 / (rn*tan(fov/2)), 0, 						0, 				0,
	0, n, 	0, 		0,				   	0,                   1 / (n * tan(fov / 2)), 	0, 				0,
	0, 0, n + f, 	1,				   	0, 					 0,							1 / (f - n), 	0,
 	0, 0, 	-fn, 	0}				   	0, 				     0,							-nf / (f - n),  1 }

则子视锥体的投影矩阵可以被推导为
Ppersp = {Swidth * m00, 	0, 				0, 	 0,
			0,			 	Sheight * m11,	0, 	 0,
			Tx,				Ty,				m22, 1,
			0,				0,				m32, 0}
*/
void ConstructFrustumPlanes(uint2 groupID, float tileMinZ, float tileMaxZ, uint2 texSize, float4x4 mat, out float4 frustumPlane[6]) {
	/*
	* 先为每个分块与计算视锥体平面，随后将结果放入一个常量缓冲区中去，在观察空间中执行，因此只有当投影矩阵发生变化时才需要重新计算
	*/
	float2 scale = float2(texSize) / TILE_GROUP_DIM;
	float2 offset = scale - 1.0f - 2.0f * float2(groupID.xy);
	// 计算当前分块视锥体的投影的矩阵
	float4 col0 = float4(scale.x * mat[0][0], 0, offset.x, 0);
	float4 col1 = float4(0, scale.y * mat[1][1], -offset.y, 0);
	float4 col3 = float4(0, 0, 1, 0);

	// Gribb/Hartmann法提取视锥体平面
	frustumPlane[0] = col3 - col0;
	frustumPlane[1] = col3 + col0;
	frustumPlane[2] = col3 - col1;
	frustumPlane[3] = col3 + col1;
	frustumPlane[4] = float4(0, 0, 1, -tileMinZ);
	frustumPlane[5] = float4(0, 0, -1, tileMaxZ);

	// 标准化视锥体平面
	[unroll]
	for (uint i = 0; i < 6; ++i){
		frustumPlane[i] *= rcp(length(frustumPlane[i].xyz));
	}
}

float3 ComputePointLight(PointLight light, MaterialData mat, float3 posV, float3 normalDir, float3 viewDir) {
	float3 lightDir = light.posV - posV;
	float dist = length(lightDir);
	lightDir = normalize(lightDir);
	float attentaution = LightAttenuation(dist, light.fallOfStart, light.fallOfEnd);
	float3 halfDir = normalize(viewDir + lightDir);

    float ndotl = dot(normalDir, lightDir);
    float ndotv = dot(normalDir, viewDir);
    float ndoth = dot(normalDir, halfDir);
    float hdotv = dot(halfDir, viewDir);

    float3 brdf = PhysicalShading(mat, ndotv, ndotl, ndoth, hdotv) * light.strength * attentaution;
	return brdf;
}

#endif