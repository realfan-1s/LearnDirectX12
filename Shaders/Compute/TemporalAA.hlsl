#ifndef TEMPORAL_ANTI_ALIASING
#define TEMPORAL_ANTI_ALIASING

#include "Struct.hlsl"
/*
* 在TAA中cbSettings w0表示jitterX, w1表示jitterY, w2表示相邻点颜色混合参数, w3表示stationaryBlur, o0表示moveBlur 
* input -> history Buffer, input1 -> currentBuffer
*/

float MaxElement(float3 col){
	return max(col.x, max(col.y, col.z));
}

float3 Map(in float3 col) {
	return float3(col.xyz * rcp(MaxElement(col) + 1.0f));
}

float3 Map(in float3 col, in float weight){
	return float3(col.xyz * rcp(weight * MaxElement(col) + 1.0f));
}

float3 Unmap(in float3 col) {
	return float3(col.xyz * rcp(1.0f - MaxElement(col)));
}

float3 ConvertToYCoCg(in float3 col) {
	float3x3 converMat = float3x3(0.25f, 0.5f, 0.25f,
						0.5f, 0.0f, -0.5f,
						-0.25f, 0.5f, -0.25f);
	float3 ans = mul(col, converMat);
	return ans;
}

float3 InverseConvertToYCoCg(in float3 col){
	float3x3 convertMat = float3x3(1.0f, 1.0f, -1.0f,
								   1.0f, 0.0f,  1.0f,
								   1.0f, -1.0f, -1.0f);
	float3 ans = mul(col, convertMat);
	return ans;
}

[numthreads(16, 16, 1)]
void Aliasing(uint2 dispatchID : SV_DISPATCHTHREADID, uint2 groupID : SV_GROUPID){
	float2 uv = dispatchID * cbInput.invTexSize;
	// 采样Motion Vector并去除Jitter
	float2 motion = motionVector.SampleLevel(anisotropicClamp, uv, 0.0f).xy;
	uv -= float2(cbInput.w0, cbInput.w1);
	const float2 k = float2(1.0f, 1.0f) * cbInput.invTexSize;
	float3 center = Map(input1.SampleLevel(anisotropicClamp, uv, 0.0f).xyz);

	// 混合采样周围点
	float3 standard = ConvertToYCoCg(center);
	float3 neighbours[8];
	neighbours[0] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(k.x, k.y), 0.0f).xyz  ));
	neighbours[1] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(k.x, -k.y), 0.0f).xyz ));
	neighbours[2] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(-k.x, k.y), 0.0f).xyz ));
	neighbours[3] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(-k.x, -k.y), 0.0f).xyz));
	neighbours[4] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(k.x, 0.0f), 0.0f).xyz ));
	neighbours[5] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(0.0f, k.y), 0.0f).xyz ));
	neighbours[6] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(-k.x, 0.0f), 0.0f).xyz));
	neighbours[7] = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, uv + float2(0.0f, -k.y), 0.0f).xyz));

	// 对历史帧颜色进行clamp
	float3 average = (center 	  + neighbours[0] + neighbours[1] +
					neighbours[2] + neighbours[3] + neighbours[4] +
					neighbours[5] + neighbours[6] + neighbours[7]) / 9.0f;
	float3 m2 = (center * center 			  + neighbours[0] * neighbours[0] + neighbours[1] * neighbours[1] +
				neighbours[2] * neighbours[2] + neighbours[3] * neighbours[3] + neighbours[4] * neighbours[4] +
				neighbours[5] * neighbours[5] + neighbours[6] * neighbours[6] + neighbours[7] * neighbours[7]) / 9.0f;
	float3 sigma = sqrt(m2 - average * average);
	float3 minColor = average - cbInput.w2 * sigma;
	float3 maxColor = average + cbInput.w2 * sigma;
	// 采样history
	float3 history = ConvertToYCoCg(Map(input.SampleLevel(anisotropicClamp, uv - motion, 0.0f).xyz));
	history = clamp(history, minColor, maxColor);
	history = InverseConvertToYCoCg(history);

	// 混合历史帧和当前帧
	float2 luma = float2(Luminance(center), Luminance(history));
	float weight = 1.0f - abs(luma.x - luma.y) / max(luma.x, max(luma.y, 0.2f));
	weight = lerp(cbInput.w3, cbInput.o0, weight * weight);
	float3 ans = lerp(center, history, weight);
	ans = Unmap(ans);
	output[dispatchID.xy] = float4(ans, 1.0f);
}

#endif