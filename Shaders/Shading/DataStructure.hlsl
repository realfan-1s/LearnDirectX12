#ifndef DATA_STRUCTURE
#define DATA_STRUCTURE

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
    uint  diffuseIndex;
    uint  normalIndex;
    uint  metalnessIndex;
    float2 gamePad0;
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

struct CascadedShadowFrustum
{
    float4 g_cascadedSpaceDepthFloat[2]; // 不同子视锥体远平面的Z值，将级联分开
    float4 g_cascadedOffset[5]; // shadowTex的平移量
    float4 g_cascadedScale[5]; // shadowTex的缩放量
    float4 g_cascadeFrustumDepthsFloat[5];
    float  g_cascadedBlendArea;
    float  g_shadowOffset;
    int    g_pcfStart;
    int    g_pcfEnd;
};

#endif