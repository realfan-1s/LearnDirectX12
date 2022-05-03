#ifndef DOWN_SAMPLER
#define DOWN_SAMPLER

#include "Struct.hlsl"

ConstantBuffer<cbSettings> cbMips : register(b0, space0);
Texture2D input : register(t0, space0);
RWTexture2D<float4> output : register(u0, space0);

float CalcLuma(float3 col);

[numthreads(16, 16, 1)]
void Down(uint3 dispatchID : SV_DISPATCHTHREADID){
	// 相当于是进行5个四次双线性纹理查询线性值
	float2 taps[13] = {float2(-1.0f, -1.0f), float2(0.0f, -1.0f), float2(1.0f, -1.0f), float2(-0.5, -0.5f),
					   float2(0.5f, -0.5f), float2(-1.0f, 0.0f), float2(0.0f, 0.0f), float2(1.0f, 0.0f),
					   float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(-1.0f, 1.0f), float2(0.0f, 1.0f), float2(1.0f, 1.0f)};

	float4 cols[13];

	cols[0]	 = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[0]) * 	cbMips.invTexSize, 0);
	cols[1]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[1]) * 	cbMips.invTexSize, 0);
	cols[2]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[2]) * 	cbMips.invTexSize, 0);
	cols[3]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[3]) * 	cbMips.invTexSize, 0);
	cols[4]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[4]) * 	cbMips.invTexSize, 0);
	cols[5]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[5]) * 	cbMips.invTexSize, 0);
	cols[6]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[6]) * 	cbMips.invTexSize, 0);
	cols[7]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[7]) * 	cbMips.invTexSize, 0);
	cols[8]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[8]) * 	cbMips.invTexSize, 0);
	cols[9]  = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[9]) * 	cbMips.invTexSize, 0);
	cols[10] = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[10]) * cbMips.invTexSize, 0);
	cols[11] = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[11]) * cbMips.invTexSize, 0);
	cols[12] = input.SampleLevel(anisotropicClamp, (dispatchID.xy + taps[12]) * cbMips.invTexSize, 0);

	half2 div = (1.0f / 4.0f) * half2(0.5f, 0.125f);
	float4 ans = float4(0.0f, 0.0f, 0.0f, 0.0f);
	ans += (cols[3] + cols[4] + cols[8] + cols[9]) * div.x;
	ans += (cols[0] + cols[5] + cols[6] + cols[1]) * div.y;
	ans += (cols[2] + cols[7] + cols[6] + cols[1]) * div.y;
	ans += (cols[12] + cols[11] + cols[6] + cols[7]) * div.y;
	ans += (cols[10] + cols[5] + cols[6] + cols[11]) * div.y;

	output[dispatchID.xy] = float4(ans.xyz, 1.0f);
}

[numthreads(16, 16, 1)]
void Up(uint3 dispatchID : SV_DISPATCHTHREADID){
	int2 offset[9] = {	int2(-1, -1), int2(0, -1), int2(1, -1),
						int2(-1, 0), int2(0, 0), int2(1, 0),
						int2(-1, 1), int2(0, 1), int2(1, 1) };

	float4 ans = float4(0.0f, 0.0f, 0.0f, 0.0f);
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[0]) * cbMips.invTexSize, 0);
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[1]) * cbMips.invTexSize, 0) * 2;
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[2]) * cbMips.invTexSize, 0);
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[3]) * cbMips.invTexSize, 0) * 2;
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[4]) * cbMips.invTexSize, 0) * 4;
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[5]) * cbMips.invTexSize, 0) * 2;
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[6]) * cbMips.invTexSize, 0) * 1;
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[7]) * cbMips.invTexSize, 0) * 2;
	ans += input.SampleLevel(linearClamp, (dispatchID.xy + offset[8]) * cbMips.invTexSize, 0);

	output[dispatchID.xy] = float4(ans.xyz * (1.0f / 16.0f), 1.0f);
}

#endif