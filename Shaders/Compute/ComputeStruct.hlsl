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
    float4x4 g_invView;
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
	float  viewZ;
    float3 worldPos;
};

struct PointLight {
    float3 strength; // 光源的颜色
    float fallOfStart; // 仅点光源、聚光灯
    float3 position; // 仅点光源、聚光灯
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

float3 CalcWorldSpacePos(float2 uv, float viewZ, float4x4 g_proj, float4x4 g_invView) {
    float3 viewPos = CalcViewSpacePos(uv, viewZ,g_proj);
    float4 worldPos = mul(float4(viewPos, 1.0f), g_invView);
    worldPos.xyz /= worldPos.w;
    return worldPos.xyz;
}

GBufferData DecodeGBuffer(Texture2D gBuffer[3], SamplerState sampleType, float2 uv, float4x4 g_proj, float4x4 g_invView) {
	GBufferData ans;
	ans.albedo = gBuffer[0].SampleLevel(sampleType, uv, 0.0f).xyz;
	float ndcZ = gBuffer[1].SampleLevel(sampleType, uv, 0.0f).x;
	float viewZ = g_proj[3][2] / (ndcZ - g_proj[2][2]);
    ans.viewZ = viewZ;
    ans.worldPos = CalcWorldSpacePos(uv, viewZ, g_proj, g_invView);
	float4 mixedParam = gBuffer[2].SampleLevel(sampleType, uv, 0.0f);
	ans.normalDir = DecodeSphereMap(mixedParam.xy);
	ans.roughness = mixedParam.z;
	ans.metalness = mixedParam.w;
	return ans;
}

#endif