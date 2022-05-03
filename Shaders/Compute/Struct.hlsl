#ifndef DATA_STRUCT
#define DATA_STRUCT

struct cbSettings
{
    // blur weight;
    float2 texSize;
    float2 invTexSize;
    float w0;
    float w1;
    float w2;
    float w3;
    float o0;
    float o1;
    float o2;
    float o3;
};

SamplerState            pointWrap        : register(s0);
SamplerState            pointClamp       : register(s1);
SamplerState            linearWrap       : register(s2);
SamplerState            linearClamp      : register(s3);
SamplerState            anisotropicWrap  : register(s4);
SamplerState            anisotropicClamp : register(s5);

#endif