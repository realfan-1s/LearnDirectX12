#include "Transform.h"
#include <D3DUtil.hpp>
#include <MathHelper.hpp>

Transform::Transform(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation)
	: m_scale(scale), m_rotation(rotation), m_position(translation)
{}

XMFLOAT4X4 Transform::GetModelMatrix(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation)
{
	XMFLOAT4X4 ans;
	XMStoreFloat4x4(&ans, GetModelMatrixXM(scale, rotation, translation));
	return ans;
}

XMMATRIX Transform::GetModelMatrixXM(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation)
{
	XMMATRIX world = XMMatrixIdentity();
	std::visit([&](const auto needScale, const auto needRotate, const auto needTranslate)
	{
		if constexpr (needScale)
		{
			const XMVECTOR scaleVec = XMLoadFloat3(&scale);
			world *= XMMatrixScalingFromVector(scaleVec);
		}
		if constexpr (needRotate)
		{
			const XMVECTOR rotateVec = XMLoadFloat3(&rotation);
			world *= XMMatrixRotationRollPitchYawFromVector(rotateVec);
		} 
		if constexpr (needTranslate)
		{
			const XMVECTOR translateVec = XMLoadFloat3(&translation);
			world *= XMMatrixTranslationFromVector(translateVec);
		}
	},
		MakeBoolVariant(!MathHelper::MathHelper::Compare(scale, XMFLOAT3(1.0f, 1.0f, 1.0f))), 
		MakeBoolVariant(!MathHelper::MathHelper::Compare(rotation, XMFLOAT3(0.0f, 0.0f, 0.0f))),
		MakeBoolVariant(!MathHelper::MathHelper::Compare(translation, XMFLOAT3(0.0f, 0.0f, 0.0f)))); 
	return world;
}

XMFLOAT4X4 Transform::GetModelMatrixInverse(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation)
{
	XMFLOAT4X4 ans;
	XMStoreFloat4x4(&ans, GetModelMatrixXMInverse(scale, rotation, translation));
	return ans;
}

XMMATRIX Transform::GetModelMatrixXMInverse(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation)
{
	XMMATRIX inverse = XMMatrixInverse(nullptr, GetModelMatrixXM(scale, rotation, translation));
	return inverse;
}

XMFLOAT3 Transform::GetUpAxis() const
{
	XMMATRIX r = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&m_rotation));
	XMFLOAT3 up;
	XMStoreFloat3(&up, r.r[1]);
	return up;
}

XMVECTOR Transform::GetUpAxisXM() const
{
	auto up = std::move(GetUpAxis());
	return XMLoadFloat3(&up);
}

XMFLOAT3 Transform::GetRightAxis() const
{
	XMMATRIX r = DirectX::XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&m_rotation));
	XMFLOAT3 right;
	XMStoreFloat3(&right, r.r[0]);
	return right;
}

XMVECTOR Transform::GetRightAxisXM() const
{
	auto right = std::move(GetRightAxis());
	return XMLoadFloat3(&right);
}

XMFLOAT3 Transform::GetForwardAxis() const
{
	XMMATRIX r = DirectX::XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&m_rotation));
	XMFLOAT3 forward;
	XMStoreFloat3(&forward, r.r[2]);
	return forward;
}

XMVECTOR Transform::GetForwardAxisXM() const
{
	auto forward = std::move(GetForwardAxis());
	return XMLoadFloat3(&forward);
}

void Transform::LookAt(const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&m_position), XMLoadFloat3(&target), XMLoadFloat3(&up));
	XMMATRIX invView = XMMatrixInverse(nullptr, view);
	XMFLOAT4X4 rotation;
	XMStoreFloat4x4(&rotation, invView);
	m_rotation = GetEulerAnglesFromRotationMatrix(rotation);
}

void Transform::Translate(const XMFLOAT3& dir, float steps)
{
	XMVECTOR direction = XMVector3Normalize(XMLoadFloat3(&dir));
	XMVECTOR pos = XMVectorMultiplyAdd(XMVectorReplicate(steps), direction, XMLoadFloat3(&m_position));
	XMStoreFloat3(&m_position, pos);
}

void Transform::Rotate(const XMFLOAT3& point, const XMFLOAT3& axis, float radians)
{
	std::visit([&](const auto isOrigin)
	{
		XMVECTOR rotate = XMLoadFloat3(&m_rotation);
		if constexpr (isOrigin)
		{
			XMMATRIX r = XMMatrixRotationRollPitchYawFromVector(rotate) * XMMatrixRotationAxis(XMLoadFloat3(&axis), radians);
			XMFLOAT4X4 rotMat;
			XMStoreFloat4x4(&rotMat, r);
			m_rotation = std::move(GetEulerAnglesFromRotationMatrix(rotMat));
		} else
		{
			XMVECTOR pos = XMLoadFloat3(&m_position);
			XMVECTOR center = XMLoadFloat3(&point);
			XMMATRIX r = XMMatrixRotationRollPitchYawFromVector(rotate) * XMMatrixTranslationFromVector(pos - center);
			r *= XMMatrixRotationAxis(XMLoadFloat3(&axis), radians);
			r *= XMMatrixTranslationFromVector(center);
			XMFLOAT4X4 rotMat;
			XMStoreFloat4x4(&rotMat, r);
			m_rotation = std::move(GetEulerAnglesFromRotationMatrix(rotMat));
			XMStoreFloat3(&m_position, r.r[3]);
		}
	}, MakeBoolVariant(MathHelper::MathHelper::Compare(point, XMFLOAT3(0, 0, 0))));
}

XMFLOAT3 Transform::GetEulerAnglesFromRotationMatrix(const XMFLOAT4X4& rotationMat)
{
	float temp = std::sqrtf(1.0f - rotationMat(2, 1) * rotationMat(2, 1));
	if (isnan(temp))
		temp = 0.0f;
	XMFLOAT3 rotate;
	rotate.x = std::atan2f(-rotationMat(2, 1), temp);
	rotate.y = std::atan2f(rotationMat(2, 0), rotationMat(2, 2));
	rotate.z = std::atan2f(rotationMat(0, 1), rotationMat(1, 1));
	return rotate;
}

