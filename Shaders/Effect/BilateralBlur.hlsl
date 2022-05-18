#ifndef BILATERAL_BLUR
#define BILATERAL_BLUR

#include "../Shading/GameBase.hlsl"
#include "../Shading/Canvas.hlsl"

struct v2f {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

v2f Vert(uint vertexID : SV_VERTEXID){
	v2f o;
	o.uv = g_uv[vertexID];
	o.pos = float4(2.0f * o.uv.x - 1.0f, 1.0f - 2.0f * o.uv.y, 1.0f, 1.0f);
	return o;
}

float Frag(v2f o) : SV_TARGET {
	float blurWeight[11] = {
		blurNoise.g_gaussW1[1], blurNoise.g_gaussW1[0], blurNoise.g_gaussW0[3], blurNoise.g_gaussW0[2],
		blurNoise.g_gaussW0[1], blurNoise.g_gaussW0[0], blurNoise.g_gaussW0[1], blurNoise.g_gaussW0[2],
		blurNoise.g_gaussW0[3], blurNoise.g_gaussW1[0], blurNoise.g_gaussW1[1]};

	float2 offsetUV = float2(0.0f, 0.0f);
	[branch]
	if (blurNoise.g_doHorizontal == 1){
		offsetUV.x = cbPass.g_invRenderTargetSize.x;
	} else {
		offsetUV.y = cbPass.g_invRenderTargetSize.y;
	}

	// 计算中心值
	float ans = blurWeight[5] * ssao.Sample(pointClamp, o.uv).x;
	float totalWeight = blurWeight[5];
	float3 centerNormal = DecodeSphereMap(gBuffer[2].Sample(pointClamp, o.uv).xy);
	float3 view_centerNormal = mul(centerNormal, (float3x3)cbPass.g_view);
	float centerNdcZ = gBuffer[1].Sample(pointClamp, o.uv).x;
	float centerViewZ = cbPass.g_proj[3][2] / (centerNdcZ - cbPass.g_proj[2][2]);

	for (int i = -5; i <= 5; ++i){
		if (i == 0)
			continue;
		float2 tex = o.uv + i * offsetUV;
		float3 neighbourNormal = DecodeSphereMap(gBuffer[2].Sample(pointClamp, tex).xy);
		float3 view_neighbourNormal = mul(neighbourNormal, (float3x3)cbPass.g_view);
		float neighbourNdcZ = gBuffer[1].Sample(gsamDepthMap, tex).x;
		float neighbourViewZ = cbPass.g_proj[3][2] / (neighbourNdcZ - cbPass.g_proj[2][2]);
		// 若中心值与邻近数值相差太大，表明处于物体边缘，不应当进行模糊处理
		if (dot(view_neighbourNormal, view_centerNormal) > 0.8f && abs(neighbourViewZ - centerViewZ) < 0.2f){
			totalWeight += blurWeight[i + 5];
			ans += (blurWeight[i + 5] * ssao.Sample(pointClamp, tex).x);
		}
	}

	return ans / totalWeight;
}

#endif