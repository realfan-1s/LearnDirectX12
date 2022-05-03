#ifndef BASE_SHADING
#define BASE_SHADING

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

struct PixelOut {
    float4 rawCol : SV_TARGET0;
    float4 bloomCol : SV_TARGET1;
};

PixelOut Frag(v2f o)
{
    Material mat = cbMaterial[o.matIndex];
    float4 sampleCol = g_modelTexture[mat.diffuseIndex].Sample(anisotropicWrap, o.uv);
#ifdef ALPHA
    clip(sampleCol.a - 0.1f);
#endif
    float3 albedo = sampleCol.xyz;
    float3 ambient = albedo * cbPass.ambient;

    /*
    * TBN
    */
    float3 normalSample = g_modelTexture[mat.normalIndex].Sample(anisotropicWrap, o.uv).xyz;
    float3 normalDir = CalcByTBN(normalSample, o.normal, o.tangent);

    float3 viewDir = normalize(cbPass.g_cameraPos - o.frag);
    MaterialData matData;
    matData.albedo = albedo;
    matData.roughness = mat.roughness * sampleCol.a;
    matData.emission = mat.emission;
    matData.metalness = mat.metalness;
    float3 ans = ComputeLighting(cbPass.lights, matData, o.frag, normalDir, viewDir, o.shadowPos) + ambient;

    PixelOut pixAns;
    float luma = CalcLuma(ans);
    pixAns.rawCol = float4(ans, sampleCol.a);
    pixAns.bloomCol = luma > 1.0f ? float4(ans, sampleCol.a) : float4(0.0f, 0.0f, 0.0f, 1.0f);
    return pixAns;
};

#endif