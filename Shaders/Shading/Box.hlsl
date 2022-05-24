#ifndef BASE_SHADING
#define BASE_SHADING

#include "GameBase.hlsl"
#include "Canvas.hlsl"
struct v2f
{
    float4 pos : SV_POSITION;
    float4 rayDir :TEXCOORD0;
    float2 uv : TEXCOORD1;
};

v2f Vert(uint vertexID : SV_VERTEXID)
{
    v2f o;
    o.uv = g_uv[vertexID];
    o.pos = float4(2.0f * o.uv.x - 1.0f, 1.0f - 2.0f * o.uv.y, 1.0f, 1.0f);

    [branch]
    if (o.uv.x < 0.5f && o.uv.y < 0.5f)
        o.rayDir = cbPass.g_viewPortRay[0];
    else if (o.uv.x > 0.5f && o.uv.y < 0.5f)
        o.rayDir = cbPass.g_viewPortRay[1];
    else if (o.uv.x > 0.5f && o.uv.y > 0.5f)
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
    float3 ambient = albedo.xyz * cbPass.ambient;
    float ndcZ = gBuffer[1].Sample(anisotropicClamp, o.uv).x;
    float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
    float4 frag = float4(cbPass.g_cameraPos + o.rayDir.xyz * (viewZ / cbPass.g_nearZ), 1.0f);
    float3 viewDir = normalize(cbPass.g_cameraPos - frag.xyz);
    float4 shadowPos = mul(frag, cbPass.shadowTansform);
    shadowPos.xyz /= shadowPos.w;
    float4 parameter = gBuffer[2].Sample(anisotropicClamp, o.uv);
    float3 normalDir = DecodeSphereMap(parameter.xy);

    MaterialData matData;
    matData.albedo = albedo.xyz;
    matData.roughness = parameter.z;
    matData.metalness = parameter.w;
    matData.emission = float3(0, 0, 0);

    float ao = ssao.Sample(anisotropicClamp, o.uv).x;
    float3 ans = ComputeLighting(cbPass.lights, matData, frag.xyz, normalDir, viewDir, shadowPos) + ambient * ao;

    PixelOut pixAns;
    float luma = Luminance(ans);
    pixAns.rawCol = float4(ans, 1.0f);
    pixAns.bloomCol = luma > 1.0f ? float4(ans, 1.0f) : float4(0.0f, 0.0f, 0.0f, 1.0f);
    return pixAns;
};

#endif