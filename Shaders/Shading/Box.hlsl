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
};


v2f Vert(input v)
{
    v2f o;
    float4 worldFrag = mul(float4(v.vertex, 1.0f), cbPerobject.g_model);
    o.frag = worldFrag.xyz;
    o.pos = mul(worldFrag, cbPass.g_vp);
    o.shadowPos = mul(worldFrag, cbPass.shadowTansform);
    o.normal = mul(v.normal, (float3x3)cbPerobject.g_model);
    o.tangent = mul(v.tangent, (float3x3)cbPerobject.g_model);
    o.uv = v.uv;
    return o;
}

float4 Frag(v2f o) : SV_TARGET
{
    Material mat = cbMaterial[cbPerobject.g_matIndex];
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
    matData.roughness = mat.roughness;
    matData.emission = mat.emission;
    matData.metalness = mat.metalness;
    float3 ans = ComputeLighting(cbPass.lights, matData, o.frag, normalDir, viewDir, o.shadowPos) + ambient;

    ans = ACESToneMapping(ans);
    ans = pow(abs(ans), INV_GAMMA_FACTOR);
    return float4(ans, sampleCol.a);
};

#endif