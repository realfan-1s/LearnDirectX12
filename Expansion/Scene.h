#pragma once

#include <DirectXCollision.h>
#include "MathHelper.hpp"

namespace Models
{
class Scene {
public:
	static constexpr BoundingSphere sceneBounds
	{XMFLOAT3(0.0f, 0.0f, 0.0f), MathHelper::MathHelper::sqrt(100.0f * 100.0f + 125.0f * 125.0f)};
private:
	using BoundingSphere = DirectX::BoundingSphere;
	using BoundingBox = DirectX::BoundingBox;
	using XMFLOAT3 = DirectX::XMFLOAT3;
};
}

