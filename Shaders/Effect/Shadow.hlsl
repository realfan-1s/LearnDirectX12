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
};

v2f Vert(input v)
{
    v2f o;
    float4 worldFrag = mul(float4(v.vertex, 1.0f), cbPerobject.g_model);
    o.pos = mul(worldFrag, cbPass.g_vp);
    o.uv = mul(float4(v.uv, 0.0f, 1.0f), cbPerobject.g_texTranform).xy;
    return o;
}

void Frag(v2f o)
{
    // 除非需要alpha几何裁剪，否则无需执行此操作
#ifdef ALPHA
    float diffuseAlpha = g_diffuse.Sample(anisotropicWrap, o.uv).a;
    clip(diffuseAlpha - 0.01f);
#endif
}

#endif