#ifndef CASCADED_SHADOW_MAP
#define CASCADED_SHADOW_MAP

#include "../Shading/DataStructure.hlsl"

static const float3 cascadeColorsMultiplier[5] =
{
    float3(1.5f, 0.0f, 0.0f),
    float3(0.0f, 1.5f, 0.0f),
    float3(0.0f, 0.0f, 5.5f),
    float3(1.5f, 0.0f, 5.5f),
    float3(1.5f, 1.5f, 0.0f),
};

/*
* 计算级联间的显示颜色
*/
float3 DebugCascadedShadowColor(uint curr, uint next, float weight) {
	return lerp(cascadeColorsMultiplier[next], cascadeColorsMultiplier[curr], weight);
}

/*
* 计算PCF滤波
*/
float CalcPCF(float4 shadowPos, float invWidth, float invHeight, CascadedShadowFrustum frustum, Texture2D shadow, SamplerComparisonState depthSample) {
	shadowPos.xyz /= shadowPos.w;
	float depthZ = shadowPos.z;
	float ans = 0.0f;
	for (int x = frustum.g_pcfStart; x <= frustum.g_pcfEnd; ++x){
		for (int y = frustum.g_pcfStart; y <= frustum.g_pcfEnd; ++y){
			float2 offset = shadowPos.xy + float2(x, y) * float2(invWidth, invHeight);
			ans += shadow.SampleCmpLevelZero(depthSample, offset, depthZ);
		}
	}
	return ans / ((frustum.g_pcfEnd - frustum.g_pcfStart + 1) * (frustum.g_pcfEnd - frustum.g_pcfStart + 1));
}

/*
* 计算两个级联之间的混合量和混合发生的范围
*/
void CalcBlendAmountForInterval(inout float depthZ, float2 shadowTexcoord, CascadedShadowFrustum frustum, uint currIdx, out float blendWeight) {
	/*      	  depth
	* 			|<-  ->|
	* __________________________
	*/
	float blendInterval = frustum.g_cascadeFrustumDepthsFloat[currIdx].x;
	if (currIdx > 0){
		int blendIdx = currIdx - 1;
		depthZ -= frustum.g_cascadeFrustumDepthsFloat[blendIdx].x;
		blendInterval - frustum.g_cascadeFrustumDepthsFloat[blendIdx].x;
	}
	// 当前像素的混合地带的位置
	float blendLocation = 1.0f - depthZ / blendInterval;
	blendWeight = blendLocation / frustum.g_cascadedBlendArea;
}

void CalcBlendAmountForMap(float4 shadowTexcoord, CascadedShadowFrustum frustum, out float blendWeight) {
	/*
	*  _________________
	* |					|
	* |		map[i+1]	|
	* |					|
	* |	   0________0	|
	* |____|	    |___|
	*	   | map[i] |
	*	   |________|
	* 	   0        0
	* blendLocation位于[0, g_blendArea]中，过渡的权重范围是[0, 1]
	*/
	float blendLocation = min(shadowTexcoord.x, min(shadowTexcoord.y, min(1.0f - shadowTexcoord.x, 1.0f - shadowTexcoord.y)));
	blendWeight = blendLocation / frustum.g_cascadedBlendArea;
}

/*
* 基于映射的级联选择：从级联0开始，计算当前级联的投影纹理坐标，然后对级联纹理的边界进行测试，倘若不在边界范围中就尝试下一层CSM，直到找到投影纹理位于边界范围的级联为止
*/
float CalcCascadedShadowByMapped(float4 shadowPos, Texture2D shadowTex[5], CascadedShadowFrustum frustum, SamplerComparisonState sampleState, out uint currIdx, out uint nextIdx, out float weight) {
	currIdx = 0, nextIdx = 0;
	weight = 1.0f;
	uint width, height;
	shadowTex[0].GetDimensions(width, height);
	float invWidth = 1.0f / (float)width, invHeight = 1.0f / (float)height;
	uint halfKernel = abs(frustum.g_pcfEnd);
	float minBorder = halfKernel * invWidth, maxBorder = 1.0f - minBorder;

	uint foundCascade = 0;
	float4 shadowTexcoord = float4(0.0f, 0.0f, 0.0f, 0.0f);
	[unroll]
	for (uint i = 0; i < 5 && !foundCascade; ++i){
		shadowTexcoord = shadowPos * frustum.g_cascadedScale[i] + frustum.g_cascadedOffset[i];
		if (min(shadowTexcoord.x, shadowTexcoord.y) > minBorder && max(shadowTexcoord.x, shadowTexcoord.y) < maxBorder){
			currIdx = i;
			foundCascade = 1;
		}
	}
	nextIdx = min(4, currIdx + 1);
	float4 shadowNextTexcoord = shadowPos * frustum.g_cascadedScale[nextIdx] + frustum.g_cascadedOffset[nextIdx];

	// 计算pcf核心
	float currPercent = CalcPCF(shadowTexcoord, invWidth, invHeight, frustum, shadowTex[currIdx], sampleState);
	float nextPercent = CalcPCF(shadowNextTexcoord, invWidth, invHeight, frustum, shadowTex[nextIdx], sampleState);
	CalcBlendAmountForMap(shadowTexcoord, frustum, weight);
	return lerp(nextPercent, currPercent, weight);
}

float CalcCascadedShadowByInterval(float4 shadowPos, float depthZ, Texture2D shadowTex[5], CascadedShadowFrustum frustum, SamplerComparisonState sampleState, out uint currIdx, out uint nextIdx, out float weight) {
	currIdx = 0, nextIdx = 0;
	weight = 1.0f;
	uint width, height;
	shadowTex[0].GetDimensions(width, height);
	float invWidth = 1.0f / width, invHeight = 1.0f / height;

	/*
	* 在该方法中，顶点着色器需要计算顶点在世界空间中的深度并在像素着色器中计算出查之后的深度并根据深度值的区间范围选择对应的级联，
	* 若是fit to scene对应的则是目标级联大的远平面内和上一级的远平面外；若是fit to cascade对应则是目标级数的近平面和远平面之间
	*/
	float4 testDepth = float4(depthZ, depthZ, depthZ, depthZ);
	// 对每一个层级的深度进行比较
	uint4 cmpVec1 = (depthZ > frustum.g_cascadedSpaceDepthFloat[0]);
	uint4 cmpVec2 = (depthZ > frustum.g_cascadedSpaceDepthFloat[1]);

	currIdx = dot(uint4(1, 1, 1, 1), cmpVec1) + dot(uint4(1, 1, 0, 0), cmpVec2);
	nextIdx = min(4, currIdx + 1);
	float4 shadowTexcoord = shadowPos * frustum.g_cascadedScale[currIdx] + frustum.g_cascadedOffset[currIdx];
	float4 shadowNextTexcoord = shadowPos * frustum.g_cascadedScale[nextIdx] + frustum.g_cascadedOffset[nextIdx];

	CalcBlendAmountForInterval(depthZ, shadowTexcoord.xy, frustum, currIdx, weight);
	float currPCF = CalcPCF(shadowTexcoord, invWidth, invHeight, frustum, shadowTex[currIdx], sampleState);
	float nextPCF = CalcPCF(shadowNextTexcoord, invWidth, invHeight, frustum, shadowTex[nextIdx], sampleState);
	return lerp(nextPCF, currPCF, weight);
}

#endif