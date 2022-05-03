#ifndef GAUSSIAN_BLUR
#define GAUSSIAN_BLUR

#include "Struct.hlsl"

#define NUM_THREADS (256)
#define MAX_BLUR_SIZE (3)
#define CACHE_SIZE (NUM_THREADS + 2 * MAX_BLUR_SIZE)

ConstantBuffer<cbSettings> cbBlur : register(b0, space0);
Texture2D input : register(t0, space0);
RWTexture2D<float4> output : register(u0, space0);

groupshared float4 cache[CACHE_SIZE];

[numthreads(NUM_THREADS, 1, 1)]
void Horizontal(uint3 groupThreadID : SV_GROUPTHREADID, uint3 dispatchID : SV_DISPATCHTHREADID){
    float weight[4] = {cbBlur.w0, cbBlur.w1, cbBlur.w2, cbBlur.w3};
    float offset[4] = {cbBlur.o0, cbBlur.o1, cbBlur.o2, cbBlur.o3};

    // 共享内存同步
    if (groupThreadID.x < MAX_BLUR_SIZE) {
        float2 left = float2((dispatchID.x - offset[MAX_BLUR_SIZE]) * cbBlur.invTexSize.x, dispatchID.y * cbBlur.invTexSize.y);
        cache[groupThreadID.x] = input.SampleLevel(anisotropicClamp, left, 0.0f);
    }
    float2 center = dispatchID.xy * cbBlur.invTexSize;
    cache[groupThreadID.x + MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, center, 0.0f);
    if (groupThreadID.x >= NUM_THREADS - MAX_BLUR_SIZE) {
        float2 right = float2((dispatchID.x + offset[MAX_BLUR_SIZE]) * cbBlur.invTexSize.x, dispatchID.y * cbBlur.invTexSize.y);
        cache[groupThreadID.x + 2 * MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, right, 0.0f);
    }
    GroupMemoryBarrierWithGroupSync();

    float4 res = float4(0, 0, 0, 0);
    uint m = groupThreadID.x + MAX_BLUR_SIZE;
    res += cache[m] * weight[0];
    [unroll]
    for (uint k = 1; k < 4; ++k){
        res += weight[k] * cache[m - k];
        res += weight[k] * cache[m + k];
    }
    output[dispatchID.xy] = res;
}

[numthreads(1, NUM_THREADS, 1)]
void Vertical(uint3 groupThreadID : SV_GROUPTHREADID, uint3 dispatchID : SV_DISPATCHTHREADID) {
    float weight[4] = {cbBlur.w0, cbBlur.w1, cbBlur.w2, cbBlur.w3};
    float offset[4] = {cbBlur.o0, cbBlur.o1, cbBlur.o2, cbBlur.o3};

    // 共享内存同步
    if (groupThreadID.y < MAX_BLUR_SIZE) {
        float2 left = float2(dispatchID.x * cbBlur.invTexSize.x, (dispatchID.y - offset[MAX_BLUR_SIZE]) * cbBlur.invTexSize.y);
        cache[groupThreadID.y] = input.SampleLevel(anisotropicClamp, left, 0.0f);
    }
    float2 center = dispatchID.xy * cbBlur.invTexSize;
    cache[groupThreadID.y + MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, center, 0.0f);
    if (groupThreadID.y >= NUM_THREADS - MAX_BLUR_SIZE) {
        float2 right = float2(dispatchID.x * cbBlur.invTexSize.x, (dispatchID.y + offset[MAX_BLUR_SIZE]) * cbBlur.invTexSize.y);
        cache[groupThreadID.y + 2 * MAX_BLUR_SIZE] = input.SampleLevel(anisotropicClamp, right, 0.0f);
    }
    GroupMemoryBarrierWithGroupSync();

    float4 res = float4(0, 0, 0, 0);
    uint m = groupThreadID.y + MAX_BLUR_SIZE;
    res += cache[m] * weight[0];
    [unroll]
    for (uint k = 1; k < 4; ++k) {
        res += weight[k] * cache[m - k];
        res += weight[k] * cache[m + k];
    }
    output[dispatchID.xy] = res;
}

#endif