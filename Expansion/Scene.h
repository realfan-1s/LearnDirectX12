#pragma once

#include <DirectXCollision.h>
#include "MathHelper.hpp"

namespace Effect
{
class Scene {
public:
	using BoundingSphere = DirectX::BoundingSphere;
	using BoundingBox = DirectX::BoundingBox;
	using XMFLOAT3 = DirectX::XMFLOAT3;
	static constexpr BoundingSphere sceneBounds
	{XMFLOAT3(0.0f, 0.0f, 0.0f), MathHelper::MathHelper::sqrt(10.0f * 10.0f + 15.0f * 15.0f)};
};
}

