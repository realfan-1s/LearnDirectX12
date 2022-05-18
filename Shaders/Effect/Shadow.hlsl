#ifndef SHADOW
#define SHADOW

#include "../Shading/Gamebase.hlsl"

struct input {
    float3 vertex : POSITION;
    float2 uv : TEXCOORD;
};

struct v2f {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    // 表示不要对该索引进行插值
    nointerpolation uint matIndex : MATINDEX;
};

v2f Vert(input v, uint instanceID : SV_INSTANCEID)
{
    v2f o;
    ObjectInstance objectData = instanceData[instanceID];
    float4 worldFrag = mul(float4(v.vertex, 1.0f), objectData.g_model);
    o.pos = mul(worldFrag, cbPass.g_vp);
    o.uv = mul(float4(v.uv, 0.0f, 1.0f), objectData.g_texTranform).xy;
    o.matIndex = objectData.g_matIndex;
    return o;
}

void Frag(v2f o)
{
    float diffuseAlpha = g_modelTexture[o.matIndex].Sample(anisotropicWrap, o.uv).a;
    clip(diffuseAlpha - 0.01f);
}

#endif