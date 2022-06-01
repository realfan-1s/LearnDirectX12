#pragma once

#include <DirectXCollision.h>
#include "MathHelper.hpp"

namespace Models
{
class Scene {
private:
	using AABB = DirectX::BoundingBox;
	using Sphere = DirectX::BoundingSphere;
	using float3 = DirectX::XMFLOAT3;
public:
	inline static AABB sceneBox{};
	static constexpr Sphere sceneBound{
		float3(0.0f, 0.0f, 0.0f), MathHelper::MathHelper::sqrt(10.0f * 10.0f + 15.0f * 15.0f)
	};
};
}

