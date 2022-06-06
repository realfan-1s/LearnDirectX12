#ifndef DATA_STRUCT
#define DATA_STRUCT

#include "ComputeStruct.hlsl"

ConstantBuffer<cbSettings> cbInput : register(b0, space0);
ConstantBuffer<ComputeConstant> cbPass : register(b1, space0);
Texture2D input : register(t0, space0);
Texture2D input1 : register(t1, space0);
Texture2D motionVector : register(t2, space0);
Texture2D gBuffer[3] : register(t3, space0);
RWTexture2D<float4> output : register(u0, space0);

SamplerState            pointWrap        : register(s0);
SamplerState            pointClamp       : register(s1);
SamplerState            linearWrap       : register(s2);
SamplerState            linearClamp      : register(s3);
SamplerState            anisotropicWrap  : register(s4);
SamplerState            anisotropicClamp : register(s5);

#endif