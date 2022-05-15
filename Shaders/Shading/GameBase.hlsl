#ifndef __GAME_BASE__
#define __GAME_BASE__

static const float PI = 3.141592653589793f;
static const float INV_PI = 0.3183098861837907f;
#define MAX_LIGHTS (16)
#define DIR_LIGHT_NUM (3)
#define SPOT_LIGHT_COUNT (0)
#define POINT_LIGHT_COUNT (0)
#define randOffset (14)

struct Light {
    float3 strength; // 光源的颜色
    float fallOfStart; // 仅点光源、聚光灯
    float3 direction; // 仅平行光源、聚光灯
    float fallOfEnd; // 仅点光源、聚光灯
    float3 position; // 仅点光源、聚光灯
    float spotPower; // 仅聚光灯
};

struct Material {
    float3 emission;
    float metalness;
    float roughness;
    uint  diffuseIndex;
    uint  normalIndex;
    uint  gobjPad0;
};

struct MaterialData {
    float3 albedo;
    float roughness;
    float3 emission;
    float metalness;
};

struct ObjectInstance // 将常量缓冲区中的元素定义在一个单独的结构体中，再用此结构体创建常量缓冲区
{
    float4x4    g_model;
    float4x4    g_texTranform;
    uint        g_matIndex;
    uint        g_objPad0;
    uint        g_objPad1;
    uint        g_objpad2;
};

struct WorldConstant
{
    float4x4 g_view;
    float4x4 g_proj;
    float4x4 g_vp;
    float4x4 g_invProj;
    float4x4 shadowTansform;
    float4x4 g_viewPortRay;
    float    g_nearZ;
    float    g_farZ;
    float    g_deltaTime;
    float    g_totalTime;
    float2   g_renderTargetSize;
    float2   g_invRenderTargetSize;
    float3   g_cameraPos;
    float    jitterX;
    float3   ambient;
    float    jitterY;
    Light    lights[MAX_LIGHTS];
};

struct SSAOPass
{
    float4 g_randNoise[randOffset];
    float  g_radius;
    float  g_surfaceEpsilon;
    float  g_attenuationEnd;
    float  g_attenuationStart;
};

struct BlurPass
{
    float4 g_gaussW0;
    float2 g_gaussW1;
    uint   g_doHorizontal;
    uint   g_pad0;
};

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
Texture2D g_shadow : register(t1);
// 将此结构化缓冲区放置在space1中，纹理数组不会与之重叠，因为这个缓冲区位于space1位置
StructuredBuffer<ObjectInstance> instanceData : register(t1, space1);
Texture2D randomTex : register(t2); // 第一个是法向半球
Texture2D ssao : register(t2, space1);
Texture2D g_modelTexture[256] : register(t3);
Texture2D gBuffer[3] : register(t3, space1);
ConstantBuffer<WorldConstant> cbPass : register(b0);
ConstantBuffer<SSAOPass> ssaoNoise : register(b1);
ConstantBuffer<BlurPass> blurNoise : register(b2);

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
float3 ComputeLighting(Light lights[MAX_LIGHTS], MaterialData mat, float3 pos, float3 normalDir, float3 viewDir);
float3 ACESToneMapping(float3 color);
float3 CalcByTBN(float3 normSample, float3 normalW, float3 tangentW);
float ShadowFilterByPCF(float4 shadowPos);
//TODO:finish PCSS
// float ShadoFilterByPCSS(float4 shadowPos);
// float FindBlocker(float3 fragToLight, float currDepth, float bias, float farPlaneSize, float3 dir);

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

    float3 specular = fresnel * GGX(ndoth, r2) * Geometry(ndotv, ndotl, r2);
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
        float shadowFactor[DIR_LIGHT_NUM] = {1.0f, 1.0f, 1.0f};
        shadowFactor[0] = ShadowFilterByPCF(shadowPos);
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

float ShadowFilterByPCF(float4 shadowPos){
    float ans = 0.0f;

    uint width, height, mipLevels;
    g_shadow.GetDimensions(0, width, height, mipLevels);
    float dx = 1.0f / width;
    const float2 offset[9] = {
        float2(-dx, -dx), float2(-dx, 0), float2(-dx, dx),
        float2(0, -dx),   float2(0, 0),   float2(0, dx),
        float2(dx, -dx),  float2(dx, 0),  float2(dx, dx)};

    [unroll]
    for (int i = 0; i < 9; ++i){
        ans += g_shadow.SampleCmp(shadowSampler, shadowPos.xy + offset[i], shadowPos.z);
    }
    ans /= 9.0f;
    return ans;
}

// float FindBlocker(float3 fragToLight, float currDepth, float bias, float farPlaneSize, float3 dir){
//     uint blocker = 0;
// 	float result = 0.0;
// 	float r = farPlaneSize * cbPass.g_nearZ / cbPass.g_farZ;

//     const float3 offset[27] = {
//         float3(-1, -1, -1), float3(-1, -1, 0), float3(-1, -1, 1),
//         float3(-1, 0, -1),  float3(-1, 0, 0),  float3(-1, 0, 1),
//         float3(-1, 1, -1),  float3(-1, 1, 0),  float3(-1, 1, 1),
//         float3(0, -1, -1),  float3(0, -1, 0),  float3(0, -1, 1),
//         float3(0, 0, -1),   float3(0, 0, 0),   float3(0, 0, 1),
//         float3(0, 1, -1),   float3(0, 1, 0),   float3(0, 1, 1),
//         float3(1, -1, -1),  float3(1, -1, 0),  float3(1, -1, 1),
//         float3(1, 0, -1),   float3(1, 0, 0),   float3(1, 0, 1),
//         float3(1, 1, -1),   float3(1, 1, 0),   float3(1, 1, 1)
//     };

//     [unroll]
// 	for (int i = 0; i < 27; ++i) {
//         currDepth /= cbPass.g_farZ;
//         currDepth -= bias;
// 		float compareRes = g_shadow.SampleCmpLevelZero(shadowSampler, dir + offset[i], currDepth).r;
//         result += compareRes;
// 		compareRes == 1.0f ? blocker++ : blocker;
// 	}
// 	return blocker == 0 ? -1 : result / blocker;

// }

// float ShadoFilterByPCSS(float4 shadowPos){
//     float ans = 0.0f;
//     return ans;
// }

float CalcLuma(float3 col) {
	return dot(col, float3(0.2126f, 0.7152f, 0.0722f));
}
#endif