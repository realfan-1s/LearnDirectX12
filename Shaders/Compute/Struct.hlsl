#ifndef DATA_STRUCT
#define DATA_STRUCT

struct cbSettings
{
    float2 texSize;
    float2 invTexSize;
    float w0;
    float w1;
    float w2;
    float w3;
    float o0;
    float o1;
    float o2;
    float o3;
};

struct ComputeConstant
{
    float4x4 g_view;
    float4x4 g_proj;
    float4x4 g_vp;
    float4x4 g_nonjitteredVP;
    float4x4 g_previousVP;
    float4x4 g_invProj;
    float4x4 g_viewPortRay;
    float    g_nearZ;
    float    g_farZ;
    float    g_deltaTime;
    float    g_totalTime;
    float3   g_cameraPos;
    float    g_GamePad0;
};

ConstantBuffer<cbSettings> cbInput : register(b0, space0);
ConstantBuffer<ComputeConstant> cbPass : register(b1, space0);
Texture2D input : register(t0, space0);
Texture2D input1 : register(t1, space0);
Texture2D motionVector : register(t2, space0);
Texture2D gBuffer[3] : register(t3, space0);
RWTexture2D<float4> output : register(u0, space0);

SamplerState            pointWrap        : register(s0);
SamplerState            pointClamp       : register(s1);
SamplerState            linearWrap       : register(s2);
SamplerState            linearClamp      : register(s3);
SamplerState            anisotropicWrap  : register(s4);
SamplerState            anisotropicClamp : register(s5);

float2 EncodeSphereMap(float3 normal){
    return normalize(normal.xy) * (sqrt(0.5f - 0.5f * normal.z));
}

float3 DecodeSphereMap(float2 encoded){
    float4 length = float4(encoded, 1.0f, -1.0f);
    float l = dot(length.xyz, -length.xyw);
    length.z = l;
    length.xy *= sqrt(l);
    return length.xyz * 2 + float3(0, 0, -1.0f);
}

float Luminance(float3 col) {
	return dot(col, float3(0.2126f, 0.7152f, 0.0722f));
}
#endif