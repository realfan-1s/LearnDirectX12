#ifndef G_BUFFER
#define G_BUFFER

#include "GameBase.hlsl"

struct input
{
	float3 vertex : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;

	nointerpolation uint matIndex : MATINDEX;
};

v2f Vert(input v, uint instanceID : SV_INSTANCEID)
{
	v2f o;
	ObjectInstance objectData = instanceData[instanceID];
	float4 worldFrag = mul(float4(v.vertex, 1.0f), objectData.g_model);
	o.pos = mul(worldFrag, cbPass.g_vp);
	o.normal = mul(v.normal, (float3x3)objectData.g_model);
	o.tangent = mul(v.tangent, (float3x3)objectData.g_model);
	o.uv = v.uv;
	o.matIndex = objectData.g_matIndex;
	return o;
}

struct GBuffer
{
	float4 gAlbedo : SV_TARGET0;
	float gDepth : SV_TARGET1;
	float4 gNormal : SV_TARGET2;
};

GBuffer Frag(v2f o)
{
	GBuffer buffer;
	Material mat = cbMaterial[o.matIndex];
	buffer.gDepth = o.pos.z;
	// 可以尝试只传输MatIndex;这样可以大幅减少带宽
	float4 sampleAlbedo = g_modelTexture[mat.diffuseIndex].Sample(anisotropicWrap, o.uv);
	clip(sampleAlbedo.a - 0.1f);
	sampleAlbedo = pow(sampleAlbedo, 2.2f);
	buffer.gAlbedo = sampleAlbedo;

	// TBN计算
	float3 normal = g_modelTexture[mat.normalIndex].Sample(anisotropicWrap, o.uv).xyz;
	float2 metalRoughtness = g_modelTexture[mat.metalnessIndex].Sample(anisotropicWrap, o.uv).xy;
	normal = CalcByTBN(normal, o.normal, o.tangent);
	buffer.gNormal.xy = EncodeSphereMap(normal);
	buffer.gNormal.zw = metalRoughtness;

	return buffer;
}

#endif