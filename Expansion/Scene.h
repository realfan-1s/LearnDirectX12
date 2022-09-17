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
	inline static Sphere sceneBound{};
};
}

