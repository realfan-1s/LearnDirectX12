#ifndef __GAME_BASE__
#define __GAME_BASE__

#include "../Effect/CascadedShadow.hlsl"

SamplerState            pointWrap        : register(s0);
SamplerState            pointClamp       : register(s1);
SamplerState            linearWrap       : register(s2);
SamplerState            linearClamp      : register(s3);
SamplerState            anisotropicWrap  : register(s4);
SamplerState            anisotropicClamp : register(s5);
SamplerComparisonState  shadowSampler    : register(s6);
SamplerState            gsamDepthMap     : register(s7);

TextureCube g_skybox : register(t0);
// 为所有的实例都创建一个存有实例的结构化缓冲区并绑定相应的数据，通过SV_InstanceID作为索引查找
StructuredBuffer<Material> cbMaterial : register(t0, space1);
// 将此结构化缓冲区放置在space1中，纹理数组不会与之重叠，因为这个缓冲区位于space1位置
StructuredBuffer<ObjectInstance> instanceData : register(t1);
Texture2D randomTex : register(t2);
Texture2D ssao : register(t3);
Texture2D g_modelTexture[256] : register(t4);
Texture2D gBuffer[3] : register(t4, space1);
Texture2D g_shadow[5] : register(t4, space2);
ConstantBuffer<WorldConstant> cbPass : register(b0);
ConstantBuffer<SSAOPass> ssaoNoise : register(b1);
ConstantBuffer<BlurPass> blurNoise : register(b2);
ConstantBuffer<CascadedShadowFrustum> csmPass : register(b3);

float GGX(float ndoth, float r2);
float Geometry(float ndotv, float ndotl, float r2);
float Geometry_Joint(float ndotv, float ndotl, float r2);
float Geometry_UE(float ndotv, float ndotl, float roughness);
float3 Fresnel(float hdotv, float3 F0);
float LightAttenuation(float dist, float fallOffStart, float fallOffEnd);
float3 PhysicalShading(MaterialData mat, float ndotv, float ndotl, float ndoth, float hdotv);
float3 ComputeDirectionalLight(Light dirLight, MaterialData mat, float3 normalDir, float3 viewDir);
float3 ComputeSpotLight(Light spotLight, MaterialData mat, float3 pos, float3 normalDir, float3 viewDir);
float3 ComputePointLight(Light pointLight, MaterialData mat, float3 pos, float3 normalDir, float3 viewDir, float r2);
float3 ComputeLighting(Light lights[MAX_LIGHTS], MaterialData mat, float3 pos, float3 normalDir, float3 viewDir, float4 shadowPos);
float3 CalcByTBN(float3 normSample, float3 normalW, float3 tangentW);
float2 EncodeSphereMap(float3 normal);
float3 DecodeSphereMap(float2 encoded);

float GGX(float ndoth, float r2){
    float r4 = r2 * r2;
    float ndoth2 = ndoth * ndoth;
    float denom = PI * (ndoth2 * (r4 - 1.0f) + 1.0f) *  (ndoth2 * (r4 - 1.0f) + 1.0f);
    return r4 / denom;
}

float Geometry(float ndotv, float ndotl, float r2){
    return 0.5f * rcp(lerp(2 * ndotv * ndotl, ndotv + ndotl, r2));
}

float Geometry_Joint(float ndotv, float ndotl, float r2){
    float Vis_V = ndotl * (ndotv * (1 - r2) + r2);
    float Vis_L = ndotv * (ndotl * (1 - r2) + r2);
    float nom = 0.5 * rcp(Vis_V + Vis_L);
    float denom = rcp(4.0f * ndotl * ndotv + 1e-5);
    return nom * denom;
}

float Geometry_UE(float ndotv, float ndotl, float roughness){
    float r = roughness + 1.0f;
	float k = r / 8.0f;
	float GL = ndotl / (ndotl * (1.0 - k) + k);
	float GV = ndotv / (ndotv * (1.0 - k) + k);
	float nom = GL* GV;
    float denom = rcp(4.0f * ndotl * ndotv + 1e-5);
    return nom * denom;
}

float3 Fresnel(float hdotv, float3 F0){
    return F0 + (1.0f - F0) * pow(1.0f - hdotv, 5.0f);
}

float3 FresnelRoughness(float ndotv, float3 f0, float roughness)
{
    return f0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), f0) - f0) * pow(1.0f - ndotv, 5.0f);
}

float LightAttenuation(float dist, float fallOffStart, float fallOffEnd){
    return saturate((fallOffEnd - dist) / (fallOffEnd - fallOffStart));
}

float3 PhysicalShading(MaterialData mat, float ndotv, float ndotl, float ndoth, float hdotv){
    float r2 = mat.roughness * mat.roughness;
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), mat.albedo, mat.metalness);
    float3 fresnel = Fresnel(hdotv, f0);

    float3 kd = float3(1.0f, 1.0f, 1.0f) - fresnel;
    kd *= 1.0f - mat.metalness;
    float3 diffuse = kd * INV_PI * mat.albedo;

    float3 specular = fresnel * GGX(ndoth, r2) * Geometry_UE(ndotv, ndotl, r2);
    float3 result = (diffuse + specular) * ndotl;
    return result;
}

float3 ComputeDirectionalLight(Light dirLight, MaterialData mat, float3 normalDir, float3 viewDir){
    float3 lightDir = normalize(-dirLight.direction);
    float3 halfDir = normalize(viewDir + lightDir);

    float ndotl = saturate(dot(normalDir, lightDir));
    float ndotv = saturate(dot(normalDir, viewDir));
    float ndoth = saturate(dot(normalDir, halfDir));
    float hdotv = saturate(dot(halfDir, viewDir));

    float3 brdf = PhysicalShading(mat, ndotv, ndotl, ndoth, hdotv) * dirLight.strength;
    return brdf;
}

float3 ComputeSpotLight(Light spotLight, MaterialData mat, float3 pos, float3 normalDir, float3 viewDir){
    float dist = length(spotLight.position - pos);
    [branch]
    if (dist > spotLight.fallOfEnd)
        return 0.0f;
    float attenuation = LightAttenuation(dist, spotLight.fallOfStart, spotLight.fallOfEnd);
    float3 lightDir = normalize(-spotLight.direction);
    float3 halfDir = normalize(viewDir + lightDir);

    float ndotl = max(0, dot(normalDir, lightDir));
    float ndotv = dot(normalDir, viewDir);
    float ndoth = dot(normalDir, halfDir);
    float hdotv = dot(halfDir, viewDir);

    float3 brdf = PhysicalShading(mat, ndotv, ndotl, ndoth, hdotv) * spotLight.strength * spotLight.spotPower;
}

float3 ComputePointLight(Light pointLight, MaterialData mat, float3 pos, float3 normalDir, float3 viewDir, float r2){
    float dist = length(pointLight.position - pos);
    if (dist > pointLight.fallOfEnd)
        return 0.0f;
    float attenuation = LightAttenuation(dist, pointLight.fallOfStart, pointLight.fallOfEnd);
    float3 lightDir = normalize(pointLight.position - pos);
    float3 halfDir = normalize(viewDir + lightDir);

    float ndotl = max(0, dot(normalDir, lightDir));
    float ndotv = dot(normalDir, viewDir);
    float ndoth = dot(normalDir, halfDir);
    float hdotv = dot(halfDir, viewDir);

    float3 brdf = PhysicalShading(mat, ndotv, ndotl, ndoth, hdotv) * pointLight.strength;
}

float3 ComputeLighting(Light lights[MAX_LIGHTS], MaterialData mat, float3 pos, float3 normalDir, float3 viewDir, float4 shadowPos){
    float3 res = 0.0f;
    int i = 0;
    #if (DIR_LIGHT_NUM > 0)
        uint currIdx, nextIdx;
        float weight;
        float shadowFactor[DIR_LIGHT_NUM] = { 1.0f, 1.0f, 1.0f };
        shadowFactor[0] = CalcCascadedShadowByMapped(shadowPos, g_shadow, csmPass, shadowSampler, currIdx, nextIdx, weight);
        [loop]
        for (; i < DIR_LIGHT_NUM; ++i){
            res += shadowFactor[i] * ComputeDirectionalLight(lights[i], mat, normalDir, viewDir);
        }
    #endif

    #if (SPOT_LIGHT_NUM > 0)
        for (; i < DIR_LIGHT_NUM + SPOT_LIGHT_NUM; ++i){
            res += ComputeSpotLight(lights[i], mat, pos, normalDir, viewDir);
        }
    #endif

    #if (POINT_LIGHT_NUM > 0)
        for (; i < MAX_LIGHTS; ++i){
            res += ComputePointLight(lights[i], mat, pos, normalDir, viewDir);
        }
    #endif
    return res + mat.emission;
}

float3 CalcByTBN(float3 normSample, float3 normalW, float3 tangentW)
{
    float3 norm = normalize(normalW);
    float3 tangent = normalize(tangentW);

    tangent = normalize(tangent - dot(tangent, norm) * norm);
    float3 bitanget = cross(norm, tangent);
    float3x3 TBN = float3x3(tangent, bitanget, norm);
    float3 normalT = 2.0f * normSample - 1.0f;
    return mul(normalT, TBN);
}

float Luminance(float3 col) {
	return dot(col, float3(0.2126f, 0.7152f, 0.0722f));
}

float2 EncodeSphereMap(float3 normal){
    return normalize(normal.xy) * (sqrt(0.5f - 0.5f * normal.z));
}

float3 DecodeSphereMap(float2 encoded){
    float4 length = float4(encoded, 1.0f, -1.0f);
    float l = dot(length.xyz, -length.xyw);
    length.z = l;
    length.xy *= sqrt(l);
    return length.xyz * 2 + float3(0, 0, -1.0f);
}
#endif