#ifndef TEMPORAL_ANTI_ALIASING
#define TEMPORAL_ANTI_ALIASING

#include "Struct.hlsl"
/*
* 在TAA中cbSettings w0表示jitterX, w1表示jitterY,
	 w2表示prevJitterX, w3表示prevJitterY, o0表示相邻点颜色混合参数, o1表示stationaryBlur, o2表示moveBlur 
* input -> history Buffer, input1 -> currentBuffer
*/

float MaxElement(float3 col){
	return max(col.x, max(col.y, col.z));
}

float3 Map(in float3 col) {
	return float3(col.xyz * rcp(MaxElement(col) + 1.0f));
}

float3 Unmap(in float3 col) {
	return float3(col.xyz * rcp(1.0f - MaxElement(col)));
}

float3 ConvertToYCoCg(in float3 col) {
	float Co = col.x - col.z;
	float temp = col.z + Co * 0.5f;
	float Cg = col.y - temp;
	float Y = temp + Cg * 0.5f;
	return float3(Y, Co, Cg);
}

float3 InverseConvertToYCoCg(in float3 col){
	float temp = col.x - col.z * 0.5f;
	float G = col.z + temp;
	float B = temp - 0.5f * col.y;
	float R = B + col.y;
	return float3(R, G, B);
}

float3 clipAABB(in float3 minCol, in float3 maxCol, float3 history, float3 average) {
	float3 center = 0.5f * (maxCol + minCol);
	float3 extent = 0.5f * (maxCol - minCol);

	float3 v_clip = history - center;
	float3 v_unit = v_clip.xyz / extent;
	float3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));
	if (ma_unit > 1.0f)
		return center + v_clip / ma_unit;
	else
		return history;
}

static const float2 offset[] = {{-1.0f, 1.0f},  {0.0f, 1.0f},  {1.0f, 1.0f},
								{-1.0f, 0.0f},                 {1.0f, 0.0f},
								{-1.0f, -1.0f}, {0.0f, -1.0f}, {1.0f, -1.0f}};

[numthreads(16, 16, 1)]
void Aliasing(uint2 dispatchID : SV_DISPATCHTHREADID){
	float2 uv = dispatchID * cbInput.invTexSize;
	// 采样Motion Vector并去除Jitter
	float2 motion = motionVector.SampleLevel(anisotropicClamp, uv, 0.0f).xy;
	float2 tex = uv - float2(cbInput.w0, cbInput.w1);
	float3 center = Map(input1.SampleLevel(anisotropicClamp, tex, 0.0f).xyz);

	// 混合采样周围点
	float3 standard = ConvertToYCoCg(center);
	float3 average = standard;
	float3 m2 = standard * standard;
	// 对历史帧颜色进行clamp
	[unroll]
	for (uint i = 0; i < 8; ++i){
		float2 neighbourUV = tex + offset[i] * cbInput.invTexSize;
		float3 neighbourCol = ConvertToYCoCg(Map(input1.SampleLevel(anisotropicClamp, neighbourUV, 0.0f).xyz));
		average += neighbourCol;
		m2 += neighbourCol * neighbourCol;
	}
	average /= 9.0f;
	m2 /= 9.0f;

	float3 sigma = sqrt(abs(m2 - average * average));
	float3 minColor = average - cbInput.o0 * sigma;
	float3 maxColor = average + cbInput.o0 * sigma;
	// 采样history
	float3 history = ConvertToYCoCg(Map(input.SampleLevel(anisotropicClamp, uv - motion, 0.0f).xyz));
	history = clipAABB(minColor, maxColor, history, average);
	history = InverseConvertToYCoCg(history);

	// 混合历史帧和当前帧
	float2 luma = float2(Luminance(center), Luminance(history));
	float weight = 1.0f - abs(luma.x - luma.y) / max(luma.x, max(luma.y, 0.2f));
	weight = lerp(cbInput.o1, cbInput.o2, weight * weight);
	float3 ans = lerp(center, history, weight);
	ans = Unmap(ans);
	output[dispatchID.xy] = float4(ans, 1.0f);
}

#endif