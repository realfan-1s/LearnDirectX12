#ifndef MOTION_VECTOR
#define MOTION_VECTOR

#include "../Shading/Canvas.hlsl"
#include "../Compute/Struct.hlsl"

struct v2f {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 rayDir : TEXCOORD1;
};

v2f Vert(uint vertexID : SV_VERTEXID) {
    v2f o;
    o.uv = g_uv[vertexID];
    o.pos = float4(2.0f * o.uv.x - 1.0f, 1.0f - 2.0f * o.uv.y, 1.0f, 1.0f);

    [branch]
    if (o.uv.x < 0.5f && o.uv.y < 0.5f)
        o.rayDir = cbPass.g_viewPortRay[0];
    else if (o.uv.x > 0.5f && o.uv.y < 0.5f)
        o.rayDir = cbPass.g_viewPortRay[1];
    else if (o.uv.x > 0.5f && o.uv.y > 0.5f)
        o.rayDir = cbPass.g_viewPortRay[2];
    else
        o.rayDir = cbPass.g_viewPortRay[3];

    return o;
}

float2 Frag(v2f o) : SV_TARGET {
	float ndcZ = gBuffer[1].Sample(anisotropicClamp, o.uv).x;
	float viewZ = cbPass.g_proj[3][2] / (ndcZ - cbPass.g_proj[2][2]);
	float4 pos = float4(cbPass.g_cameraPos + o.rayDir.xyz * (viewZ / cbPass.g_nearZ), 1.0f);
	float4 currUV = mul(pos, cbPass.g_vp);
	currUV.xyz /= currUV.w;
	currUV.xy -= float2(cbInput.w2, cbInput.w3);
	float4 prevUV = mul(pos, cbPass.g_previousVP);
	prevUV.xyz /= prevUV.w;
	prevUV.xy -= float2(cbInput.w0, cbInput.w1);
	return currUV.xy - prevUV.xy;
}

#endif