#ifndef BASCIAL_BRDF
#define BASCIAL_BRDF

#include "DataStructure.hlsl"

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

#endif