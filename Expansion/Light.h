#pragma once

#include <DirectXMath.h>
#include <memory>

struct LightData 
{
	DirectX::XMFLOAT3	strength;
	float				fallOffStart;
	DirectX::XMFLOAT3	direction;
	float				fallOffEnd;
	DirectX::XMFLOAT3	position;
	float				spotPower;
};

enum LightType {
	directional = 0,
	spot = 1,
	point = 2
};

class Light
{
public:
	Light(const LightData& _data);
	Light(LightData&& _data);
	~Light() = default;
	Light(const Light&) = default;
	Light& operator=(const Light&) = default;
	Light(Light&&) = default;
	Light& operator=(Light&&) = default;
	DirectX::XMVECTOR GetLightDir() const;
	const LightData& GetData() const;
	DirectX::XMVECTOR GetLightPos() const;
protected:
	std::shared_ptr<LightData> m_lightData;
};