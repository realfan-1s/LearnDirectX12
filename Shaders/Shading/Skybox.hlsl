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

v2f Vert(Input v, uint instanceID : SV_INSTANCEID) {
    v2f o;
    ObjectInstance objectData = instanceData[instanceID];
    // 将输入的位置作为输出的纹理坐标
    o.frag = v.vertex;
    float4 worldPos = mul(float4(v.vertex, 1.0f), objectData.g_model);
    // 需要移除所有位移但保留所有旋转变换
    worldPos.xyz += cbPass.g_cameraPos;
    // 为了欺骗深度缓冲，让其认为天空盒有者最大的深度值0.0f以寻求提前深度测试
    o.pos = mul(worldPos, cbPass.g_vp);
    o.pos.z = 0.0f;
    return o;
}

struct PixelOut {
    float4 rawCol : SV_TARGET0;
    float4 bloomCol : SV_TARGET1;
};

PixelOut Frag(v2f o) {
    float3 ans = g_skybox.Sample(anisotropicWrap, o.frag).xyz;
    PixelOut pixAns;
    float luma = Luminance(ans);
    pixAns.rawCol = float4(ans, 1.0f);
    pixAns.bloomCol = luma > 1.0f ? float4(ans, 1.0f) : float4(0.0f, 0.0f, 0.0f, 1.0f);
    return pixAns;
}

#endif