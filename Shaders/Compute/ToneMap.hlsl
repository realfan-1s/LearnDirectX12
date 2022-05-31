#ifndef TONE_MAP
#define TONE_MAP

#include "Struct.hlsl"

#define GAMMA_FACTOR 2.2f
#define INV_GAMMA_FACTOR (1.0f / GAMMA_FACTOR)

float3 ACESToneMapping(float3 col);

[numthreads(16, 16, 1)]
void ACES(uint3 dispatchID : SV_DISPATCHTHREADID) {
	// float4 col = input[dispatchID.xy] + input1[dispatchID.xy];
	// float3 ans = ACESToneMapping(col.xyz);
	// ans = pow(abs(ans), INV_GAMMA_FACTOR);
    float3 ans = input[dispatchID.xy].xyz;
	output[dispatchID.xy] = float4(ans, 1.0f);
}

float3 ACESToneMapping(float3 col){
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    return (col * (A * col + B)) / (col * (C * col + D) + E);
}


#endif