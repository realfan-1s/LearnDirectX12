#ifndef DEBUG_MOTION_VECTOR
#define DEBUG_MOTION_VECTOR

#include "../Shading/Canvas.hlsl"

struct Input {
	float3 vertex : POSITION;
	float2 uv : TEXCOORD;
};

struct v2f {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D 				debugTex 		 : register(t0);
SamplerState            pointWrap        : register(s0);
SamplerState            pointClamp       : register(s1);
SamplerState            linearWrap       : register(s2);
SamplerState            linearClamp      : register(s3);
SamplerState            anisotropicWrap  : register(s4);
SamplerState            anisotropicClamp : register(s5);

v2f Vert(Input i) {
	v2f o;
	o.pos = float4(i.vertex, 1.0f);
	o.uv = i.uv;
	return o;
}

float4 Frag(v2f o) : SV_TARGET {
	float3 ans = debugTex.Sample(anisotropicWrap, o.uv).xyz;
	return float4(ans, 1.0f);
}

#endif