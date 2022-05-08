#ifndef BASE_SHADING
#define BASE_SHADING

#include "GameBase.hlsl"

static const float2 g_uv[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};
struct v2f
{
    float4 pos : SV_POSITION;
    float4 rayDir :TEXCOORD0;
    float2 uv : TEXCOORD1;
};

v2f Vert(uint vertexID : SV_VERTEXID)
{
    v2f o;
    float2 tex = g_uv[vertexID];
    o.uv = tex;
    o.pos = float4(2.0f * o.uv.x - 1.0f, 1.0f - 2.0f * o.uv.y, 0.0f, 1.0f);

    if (tex.x < 0.5f && tex.y < 0.5f)
        o.rayDir = cbPass.g_viewPortRay[0];
    else if (tex.x > 0.5f && tex.y < 0.5f)
        o.rayDir = cbPass.g_viewPortRay[1];
    else if (tex.x > 0.5f && tex.y > 0.5f)
        o.rayDir = cbPass.g_viewPortRay[2];
    else
        o.rayDir = cbPass.g_viewPortRay[3];

    return o;
}

struct PixelOut {
    float4 rawCol : SV_TARGET0;
    float4 bloomCol : SV_TARGET1;
};

PixelOut Frag(v2f o)
{
    float4 albedo = gBuffer[0].Sample(anisotropicClamp, o.uv);
#ifdef ALPHA
    clip(albedo.a - 0.1f);
#endif
    float3 ambient = albedo.xyz * cbPass.ambient;
    float ndcZ = gBuffer[1].Sample(gsamDepthMap, o.uv).x;
    float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
    float4 frag = float4(cbPass.g_cameraPos + o.rayDir.xyz * viewZ / cbPass.g_nearZ, 1.0f);
    float4 shadowPos = mul(frag, cbPass.shadowTansform);
    float3 viewDir = normalize(cbPass.g_cameraPos - frag.xyz);
    float4 parameter = gBuffer[2].Sample(anisotropicClamp, o.uv);
    float3 normalDir = parameter.xyz;

    MaterialData matData;
    matData.albedo = albedo.xyz;
    matData.roughness = albedo.w;
    matData.emission = float3(0, 0, 0);
    matData.metalness = parameter.w;

    float3 ans = ComputeLighting(cbPass.lights, matData, frag.xyz, normalDir, viewDir, shadowPos) + ambient;

    PixelOut pixAns;
    float luma = CalcLuma(ans);
    pixAns.rawCol = float4(ans, 1.0f);
    pixAns.bloomCol = luma > 1.0f ? float4(ans, 1.0f) : float4(0.0f, 0.0f, 0.0f, 1.0f);
    return pixAns;
};

#endif