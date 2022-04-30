#ifndef DOWN_SAMPLER
#define DOWN_SAMPLER

Texture2D input : register(t0, space0);
RWTexture2D<float4> output : register(u0, space0);

float CalcLuma(float3 col);

[numthreads(HORIZONTAL_GROUP, VECTRICAL_GROUP, 1)]
void Down(uint3 dispatchID : SV_DISPATCHTHREADID){
	float3 texs[16];
	float totalLuma = 0.0f;
	[unroll]
	for (uint x = 0; x < 4; ++x){
		[unroll]
		for (uint y = 0; y < 4; ++y){
			uint2 uv = 4 * dispatchID.xy + uint2(x, y);
			float3 col = input[uv].xyz;
			float luma = CalcLuma(col);
			texs[4 * x + y] = col * luma;
			totalLuma += luma;
		}
	}

	float3 ans = float3(0.0f, 0.0f, 0.0f);
	[unroll]
	for (uint i = 0; i < 4; ++i){
		[unroll]
		for (uint j = 0; j < 4; ++j){
			float3 weights = texs[4 * i + j] / totalLuma;
			ans += weights;
		}
	}

	output[dispatchID.xy] = float4(ans, 1.0f);
}

[numthreads(HORIZONTAL_GROUP, VECTRICAL_GROUP, 1)]
void Up(uint3 dispatchID : SV_DISPATCHTHREADID){
	int2 rowPos = dispatchID.xy / 4;
	int2 offset[9] = {	int2(-1, -1), int2(0, -1), int2(1, -1),
						int2(-1, 0), int2(0, 0), int2(1, 0),
						int2(-1, 1), int2(0, 1), int2(1, 1) };

	uint2 size;
	input.GetDimensions(size.x, size.y);
	size -= uint2(1, 1);
	int2 start = int2(0, 0);

	float4 ans = float4(0.0f, 0.0f, 0.0f, 0.0f);
	ans += input[clamp(rowPos + offset[0], start, size)];
	ans += input[clamp(rowPos + offset[1], start, size)] * 2;
	ans += input[clamp(rowPos + offset[2], start, size)];
	ans += input[clamp(rowPos + offset[3], start, size)] * 2;
	ans += input[clamp(rowPos + offset[4], start, size)] * 4;
	ans += input[clamp(rowPos + offset[5], start, size)] * 2;
	ans += input[clamp(rowPos + offset[6], start, size)] * 1;
	ans += input[clamp(rowPos + offset[7], start, size)] * 2;
	ans += input[clamp(rowPos + offset[8], start, size)];

	output[dispatchID.xy] = float4(ans.xyz * (1.0f / 16.0f), 1.0f);
}

float CalcLuma(float3 col) {
	return dot(col, float3(0.2126f, 0.7152f, 0.0722f));
}

#endif