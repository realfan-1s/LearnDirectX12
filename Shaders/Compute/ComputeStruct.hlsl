#ifndef COMPUTE_DATA_STRUCTURE
#define COMPUTE_DATA_STRUCTURE

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
    float4x4 g_viewPortRay;
    float    g_nearZ;
    float    g_farZ;
    float    g_deltaTime;
    float    g_totalTime;
    float3   g_cameraPos;
    float    g_GamePad0;
};

struct GBufferData {
	float3 albedo;
	float3 normalDir;
	float  roughness;
	float  metalness;
    float3 viewPos;
};

struct PointLight {
    float3 strength; // 光源的颜色
    float fallOfStart; // 仅点光源、聚光灯
    float3 posV; // 仅点光源、聚光灯
    float fallOfEnd; // 仅点光源、聚光灯
};

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

float3 CalcViewSpacePos(float2 uv, float viewZ, float4x4 g_proj) {
    float2 screenRay = float2(uv.x / g_proj[0][0], uv.y / g_proj[1][1]);
    float3 viewPos;
    viewPos.z = viewZ;
    viewPos.xy = screenRay * viewZ;
    return viewPos;
}

GBufferData DecodeGBuffer(Texture2D gBuffer[3], SamplerState sampleType, uint2 dispatchID, float2 invSize, float4x4 g_proj, float4x4 g_view) {
	float2 uv = dispatchID * invSize;
    // 视口变换和像素中心位于(x+0.5, y+0.5)位置
    float2 pixelOffset = float2(2.0f, -2.0f) * invSize;
    float2 posNDC = (float2(dispatchID.xy) + 0.5f) * pixelOffset + float2(-1.0f, 1.0f);
    GBufferData ans;
	ans.albedo = gBuffer[0].SampleLevel(sampleType, uv, 0.0f).xyz;
	float ndcZ = gBuffer[1].SampleLevel(sampleType, uv, 0.0f).x;
	float viewZ = g_proj[3][2] / (ndcZ - g_proj[2][2]);
    ans.viewPos = CalcViewSpacePos(posNDC, viewZ, g_proj);
	float4 mixedParam = gBuffer[2].SampleLevel(sampleType, uv, 0.0f);
    float4 normalDir = mul(float4(DecodeSphereMap(mixedParam.xy), 0.0f), g_view);
    ans.normalDir = normalDir.xyz;
	ans.roughness = mixedParam.z;
	ans.metalness = mixedParam.w;
	return ans;
}

#endif