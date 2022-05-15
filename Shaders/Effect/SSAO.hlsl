#ifndef SSAO
#define SSAO

#include "../Shading/GameBase.hlsl"
#include "../Shading/Canvas.hlsl"

float OcclusionFunc(float distZ);

struct v2f {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
	float3 frag : TEXCOORD1;
};

v2f Vert(uint vertexID : SV_VERTEXID){
    v2f o;
    o.uv = g_uv[vertexID];
    o.pos = float4(2.0f * o.uv.x - 1.0f, 1.0f - 2.0f * o.uv.y, 1.0f, 1.0f);

	float4 viewPos = mul(o.pos, cbPass.g_invProj);
    o.frag = viewPos.xyz / viewPos.w;
	return o;
}

float Frag(v2f o) : SV_TARGET {
	float ans = 0.0f;
	// 将法线坐标和空间坐标都以观察空间的格式输出
	float3 normalDir = gBuffer[2].Sample(anisotropicClamp, o.uv).xyz;
	normalDir = mul(normalDir, (float3x3)cbPass.g_view);

	float ndcZ = gBuffer[1].Sample(anisotropicClamp, o.uv).x;
	float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
	float3 viewPos = (viewZ / o.frag.z) * o.frag;
	float3 randSeed = 2.0f * randomTex.Sample(anisotropicWrap, 4.0f * o.uv).xyz - 1.0f;

	// 遮蔽点比较
	for (uint i = 0; i < randOffset; ++i){
		// 均匀分布且固定的偏移向量，关于随机向量反射后比能得到均匀分布的随机偏移向量
		float3 offset = reflect(ssaoNoise.g_randNoise[i].xyz, randSeed);
		// 避免陷入z<0的负半球中
		float filp = sign(dot(offset, normalDir));
		float3 randP = viewPos + filp * offset * ssaoNoise.g_radius;
		// 采样随机点位置的深度值
		float4 projP = mul(float4(randP, 1.0f), cbPass.g_proj);
		projP.xyz /= projP.w;
		float reconstructZ = gBuffer[1].Sample(gsamDepthMap, projP.xy).x;
		reconstructZ = cbPass.g_proj[3][2] / (reconstructZ - cbPass.g_proj[2][2]);
		float3 reconstructPos = (reconstructZ / randP.z) * randP;

		/*
		 测试点r是否遮蔽：
			(1) depth_p - depth_r 足够小到被遮蔽
			(2) max(dot(normal, ||r - p||), 0) > 0
		*/
		float distZ = viewPos.z - reconstructZ;
		float dp = max(dot(normalDir, normalize(reconstructPos - viewPos)), 0.0f);
		ans += dp * OcclusionFunc(distZ);
	}

	ans /= randOffset;
	float access = 1.0f - ans;
	return saturate(pow(access, 2.0f));
}

float OcclusionFunc(float distZ){
	float occlusion = 0.0f;

	if (distZ > ssaoNoise.g_surfaceEpsilon)
	{
		float dist = ssaoNoise.g_attenuationEnd - ssaoNoise.g_attenuationStart;
		occlusion = saturate((ssaoNoise.g_attenuationEnd - distZ) / dist);
	}

	return occlusion;
}

#endif