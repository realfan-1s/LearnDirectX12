#include "Light.h"

Light::Light(const LightData& _data)
: m_lightData(std::make_shared<LightData>(_data))
{
}

Light::Light(LightData&& _data)
: m_lightData(std::make_shared<LightData>(_data))
{
}

DirectX::XMVECTOR Light::GetLightDir() const
{
	return DirectX::XMLoadFloat3(&m_lightData->direction);
}

const LightData& Light::GetData() const
{
	return *m_lightData.get();
}

DirectX::XMVECTOR Light::GetLightPos() const
{
	return DirectX::XMLoadFloat3(&m_lightData->position);
}
