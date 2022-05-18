#ifndef FORWARD
#define forwrad;

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
    float3 frag : POSITION0;
    float4 shadowPos : POSITION1;
    float2 uv : TEXCOORD;

    nointerpolation uint matIndex : MATINDEX;
};


v2f Vert(input v, uint instanceID : SV_INSTANCEID)
{
    v2f o;
    ObjectInstance objectData = instanceData[instanceID];
    float4 worldFrag = mul(float4(v.vertex, 1.0f), objectData.g_model);
    o.frag = worldFrag.xyz;
    o.pos = mul(worldFrag, cbPass.g_vp);
    o.shadowPos = mul(worldFrag, cbPass.shadowTansform);
    o.normal = mul(v.normal, (float3x3)objectData.g_model);
    o.tangent = mul(v.tangent, (float3x3)objectData.g_model);
    o.matIndex = objectData.g_matIndex;
    o.uv = v.uv;
    return o;
}

float4 Frag(v2f o) : SV_TARGET
{
    Material mat = cbMaterial[o.matIndex];
    float4 sampleCol = g_modelTexture[mat.diffuseIndex].Sample(anisotropicWrap, o.uv);
    clip(sampleCol.a - 0.1f);
    float3 albedo = sampleCol.xyz;
    float3 ambient = albedo * cbPass.ambient;

    /*
    * TBN
    */
    float3 normalSample = g_modelTexture[mat.normalIndex].Sample(anisotropicWrap, o.uv).xyz;
    float3 normalDir = CalcByTBN(normalSample, o.normal, o.tangent);
    float2 metalicRoughness = g_modelTexture[mat.metalnessIndex].Sample(anisotropicWrap, o.uv).xy;

    float3 viewDir = normalize(cbPass.g_cameraPos - o.frag);
    MaterialData matData;
    matData.albedo = albedo;
    matData.roughness = metalicRoughness.x;
    matData.metalness = metalicRoughness.y;
    matData.emission = mat.emission;
    float3 ans = ComputeLighting(cbPass.lights, matData, o.frag, normalDir, viewDir, o.shadowPos) + ambient;

    return float4(ans, sampleCol.a);
};

#endif