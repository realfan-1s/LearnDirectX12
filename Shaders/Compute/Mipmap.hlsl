#ifndef MIP_MAP
#define MIP_MAP

#include "Struct.hlsl"

[numthreads(16, 16, 1)]
void Generate(uint3 dispatchID : SV_DISPATCHTHREADID){
	// 通过当前的dispatchID查找到原先UV的位置
	float2 uv = cbInput.invTexSize * (dispatchID.xy + float2(0.5f, 0.5f));
	float4 color = input.SampleLevel(linearClamp, uv, 0);
	output[dispatchID.xy] = float4(color.xyz, 1.0f);
}

#endif