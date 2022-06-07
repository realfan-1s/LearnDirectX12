#pragma once

#include <DirectXMath.h>
#include <memory>

struct LightInPixel 
{
	DirectX::XMFLOAT3	strength;
	float				fallOffStart;
	DirectX::XMFLOAT3	direction;
	float				fallOffEnd;
	DirectX::XMFLOAT3	position;
	float				spotPower;
};

struct  LightInCompute {
	DirectX::XMFLOAT3	strength;
	float				fallOffStart;
	DirectX::XMFLOAT3	posV;
	float				fallOffEnd;
};

struct LightMoveParams {
	float radius;
	float angle;
	float height;
	float moveSpeed;
};

enum LightType {
	Pixel = 0,
	Compute = 1
};

template <LightType T, typename U = int>
class Light {};

template <LightType T>
class Light<T, std::enable_if_t<T == Pixel, int>>
{
public:
	Light(const LightInPixel& _data)
	:m_lightData(std::make_shared<LightInPixel>(_data)) {}
	Light(LightInPixel&& _data)
	:m_lightData(std::make_shared<LightInPixel>(_data)) {}
	~Light() = default;
	Light(const Light&) = default;
	Light& operator=(const Light&) = default;
	Light(Light&&) = default;
	Light& operator=(Light&&) = default;
	DirectX::XMVECTOR GetLightDir() const {
		return DirectX::XMLoadFloat3(&m_lightData->direction);
	}
	const LightInPixel& GetData() const {
		return *m_lightData.get();
	}
	DirectX::XMVECTOR GetLightPos() const {
		return DirectX::XMLoadFloat3(&m_lightData->position);
	}
protected:
	std::shared_ptr<LightInPixel> m_lightData;
};

template <LightType T>
class Light<T, std::enable_if_t<T == Compute, int>>
{
public:
	Light(const LightInCompute& _data)
		:m_lightData(std::make_shared<LightInCompute>(_data)) {}
	Light(LightInCompute&& _data)
		:m_lightData(std::make_shared<LightInCompute>(_data)) {}
	~Light() = default;
	Light(const Light&) = default;
	Light& operator=(const Light&) = default;
	Light(Light&&) = default;
	Light& operator=(Light&&) = default;
	void MovePos(const XMFLOAT3& pos) const
	{
		m_lightData->posV = std::move(pos);
	}
	void MovePos(const XMVECTOR& pos) const
	{
		XMStoreFloat3(&m_lightData->posV, pos);
	}
	const LightInCompute& GetData() const {
		return *m_lightData.get();
	}
	DirectX::XMVECTOR GetLightPos() const {
		return DirectX::XMLoadFloat3(&m_lightData->posV);
	}
protected:
	std::shared_ptr<LightInCompute> m_lightData;
};