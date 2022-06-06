#ifndef BILATERAL_BLUR
#define BILATERAL_BLUR

#include "ComputeBase.hlsl"

#define NUM_THREADS (256)
#define MAX_BLUR_SIZE (3)
#define CACHE_SIZE (NUM_THREADS + 2 * MAX_BLUR_SIZE)

groupshared float4 cache[CACHE_SIZE];
groupshared float4 gBufferCache[CACHE_SIZE];

[numthreads(NUM_THREADS, 1, 1)]
void Horizontal(uint3 groupThreadID : SV_GROUPTHREADID, uint3 dispatchID : SV_DISPATCHTHREADID) {
    float weight[4] = {cbInput.w0, cbInput.w1, cbInput.w2, cbInput.w3};
    float offset[4] = {cbInput.o0, cbInput.o1, cbInput.o2, cbInput.o3};

    // 共享内存同步
    if (groupThreadID.x < MAX_BLUR_SIZE) {
        float2 left = float2((dispatchID.x - offset[MAX_BLUR_SIZE]) * cbInput.invTexSize.x, dispatchID.y * cbInput.invTexSize.y);
        cache[groupThreadID.x] = input.SampleLevel(anisotropicClamp, left, 0.0f);
		// Cache GBuffer
		float3 normalDir = DecodeSphereMap(gBuffer[2].SampleLevel(anisotropicClamp, left, 0.0f).xy);
		gBufferCache[groupThreadID.x].xyz = mul(normalDir, (float3x3)cbPass.g_view);
		float ndcZ = gBuffer[1].SampleLevel(anisotropicClamp, left, 0.0f).x;
		float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
		gBufferCache[groupThreadID.x].w = viewZ;
    }
    float2 center = dispatchID.xy * cbInput.invTexSize;
    cache[groupThreadID.x + MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, center, 0.0f);
	// Cache GBuffer Normal
	float3 normalDir = DecodeSphereMap(gBuffer[2].SampleLevel(anisotropicClamp, center, 0.0f).xy);
	gBufferCache[groupThreadID.x + MAX_BLUR_SIZE].xyz = mul(normalDir, (float3x3)cbPass.g_view);
	// Cache GBuffer Depth
	float ndcZ = gBuffer[1].SampleLevel(anisotropicClamp, center, 0.0f).x;
	float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
	gBufferCache[groupThreadID.x + MAX_BLUR_SIZE].w = viewZ;
    if (groupThreadID.x >= NUM_THREADS - MAX_BLUR_SIZE) {
        float2 right = float2((dispatchID.x + offset[MAX_BLUR_SIZE]) * cbInput.invTexSize.x, dispatchID.y * cbInput.invTexSize.y);
        cache[groupThreadID.x + 2 * MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, right, 0.0f);
		// Cache GBuffer Normal
		float3 normalDir = DecodeSphereMap(gBuffer[2].SampleLevel(anisotropicClamp, right, 0.0f).xy);
		gBufferCache[groupThreadID.x + 2 * MAX_BLUR_SIZE].xyz = mul(normalDir, (float3x3)cbPass.g_view);
		// Cache GBuffer Depth
		float ndcZ = gBuffer[1].SampleLevel(anisotropicClamp, right, 0.0f).x;
		float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
		gBufferCache[groupThreadID.x + 2 * MAX_BLUR_SIZE].w = viewZ;
    }
    GroupMemoryBarrierWithGroupSync();

    float4 res = float4(0, 0, 0, 0);
	float totalWeight = weight[0];
    uint m = groupThreadID.x + MAX_BLUR_SIZE;
    res += cache[m] * weight[0];
	float3 centerNormal = gBufferCache[m].xyz;
	float centerDepth = gBufferCache[m].w;
    [unroll]
    for (uint k = 1; k < 4; ++k) {
		float3 neighbourNormal0 = gBufferCache[m - k].xyz;
		float  neighbourDepth0 = gBufferCache[m - k].w;
		float3 neighbourNormal1 = gBufferCache[m + k].xyz;
		float  neighbourDepth1 = gBufferCache[m + k].w;
		if (dot(neighbourNormal0, centerNormal) > 0.8f && abs(neighbourDepth0 - centerDepth) < 0.2f) {
        	res += weight[k] * cache[m - k];
			totalWeight += weight[k];
		}
		if (dot(neighbourNormal1, centerNormal) > 0.8f && abs(neighbourDepth1 - centerDepth) < 0.2f) {
        	res += weight[k] * cache[m + k];
			totalWeight += weight[k];
		}
    }

	output[dispatchID.xy] = res / totalWeight;
}

[numthreads(1, NUM_THREADS, 1)]
void Vertical(uint3 groupThreadID : SV_GROUPTHREADID, uint3 dispatchID : SV_DISPATCHTHREADID) {
    float weight[4] = {cbInput.w0, cbInput.w1, cbInput.w2, cbInput.w3};
    float offset[4] = {cbInput.o0, cbInput.o1, cbInput.o2, cbInput.o3};

    // 共享内存同步
    if (groupThreadID.y < MAX_BLUR_SIZE) {
        float2 left = float2(dispatchID.x * cbInput.invTexSize.x, (dispatchID.y - offset[MAX_BLUR_SIZE]) * cbInput.invTexSize.y);
        cache[groupThreadID.y] = input.SampleLevel(anisotropicClamp, left, 0.0f);
		// Cache GBuffer Normal
		float3 normalDir = gBuffer[2].SampleLevel(anisotropicClamp, left, 0.0f).xyz;
		gBufferCache[groupThreadID.y].xyz = mul(normalDir, (float3x3)cbPass.g_view);
		// Cache GBuffer Depth
		float ndcZ = gBuffer[1].SampleLevel(anisotropicClamp, left, 0.0f).x;
		float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
		gBufferCache[groupThreadID.y].w = viewZ;
    }
    float2 center = dispatchID.xy * cbInput.invTexSize;
    cache[groupThreadID.y + MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, center, 0.0f);
	// Cache GBuffer Normal
	float3 normalDir = gBuffer[2].SampleLevel(anisotropicClamp, center, 0.0f).xyz;
	gBufferCache[groupThreadID.y + MAX_BLUR_SIZE].xyz = mul(normalDir, (float3x3)cbPass.g_view);
	// Cache GBuffer Depth
	float ndcZ = gBuffer[1].SampleLevel(anisotropicClamp, center, 0.0f).x;
	float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
	gBufferCache[groupThreadID.y + MAX_BLUR_SIZE].w = viewZ;
    if (groupThreadID.y >= NUM_THREADS - MAX_BLUR_SIZE) {
        float2 right = float2(dispatchID.x * cbInput.invTexSize.x, (dispatchID.y + offset[MAX_BLUR_SIZE]) * cbInput.invTexSize.y);
        cache[groupThreadID.y + 2 * MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, right, 0.0f);
		// Cache GBuffer Normal
		float3 normalDir = gBuffer[2].SampleLevel(anisotropicClamp, right, 0.0f).xyz;
		gBufferCache[groupThreadID.y + 2 * MAX_BLUR_SIZE].xyz = mul(normalDir, (float3x3)cbPass.g_view);
		// Cache GBuffer Depth
		float ndcZ = gBuffer[1].SampleLevel(anisotropicClamp, right, 0.0f).x;
		float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
		gBufferCache[groupThreadID.y + 2 * MAX_BLUR_SIZE].w = viewZ;
    }
    GroupMemoryBarrierWithGroupSync();

	float4 res = float4(0, 0, 0, 0);
	float totalWeight = weight[0];
    uint m = groupThreadID.y + MAX_BLUR_SIZE;
    res += cache[m] * weight[0];
	float3 centerNormal = gBufferCache[m].xyz;
	float centerDepth = gBufferCache[m].w;
    [unroll]
    for (uint k = 1; k < 4; ++k) {
		float3 neighbourNormal0 = gBufferCache[m - k].xyz;
		float  neighbourDepth0 = gBufferCache[m - k].w;
		float3 neighbourNormal1 = gBufferCache[m + k].xyz;
		float  neighbourDepth1 = gBufferCache[m + k].w;
		if (dot(neighbourNormal0, centerNormal) > 0.8f && abs(neighbourDepth0 - centerDepth) < 0.2f) {
        	res += weight[k] * cache[m - k];
			totalWeight += weight[k];
		}
		if (dot(neighbourNormal1, centerNormal) > 0.8f && abs(neighbourDepth1 - centerDepth) < 0.2f) {
        	res += weight[k] * cache[m + k];
			totalWeight += weight[k];
		}
    }

	output[dispatchID.xy] = res / totalWeight;
}

#endif