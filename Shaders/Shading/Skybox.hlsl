#ifndef SKYBOX
#define SKYBOX

#include "GameBase.hlsl"

struct Input {
    float3 vertex : POSITION;
    float2 uv : TEXCOORD;
};

struct v2f {
    float4 pos : SV_POSITION;
    float3 frag : POSITION;
};

v2f Vert(Input v) {
    v2f o;
    // 将输入的位置作为输出的纹理坐标
    o.frag = v.vertex;
    float4 worldPos = mul(float4(v.vertex, 1.0f), cbPerobject.g_model);
    // 需要移除所有位移但保留所有旋转变换
    worldPos.xyz += cbPass.g_cameraPos;
    // 为了欺骗深度缓冲，让其认为天空盒有者最大的深度值1.0f以寻求提前深度测试
    o.pos = mul(worldPos, cbPass.g_vp).xyww;
    return o;
}

float4 Frag(v2f o) : SV_TARGET {
    float3 skybox = g_skybox.Sample(anisotropicWrap, o.frag).xyz;
    float3 ans = ACESToneMapping(skybox);
    return float4(ans, 1.0f);
}

#endif