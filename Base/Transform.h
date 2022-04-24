#pragma once

#include <DirectXMath.h>

using namespace  DirectX;

struct Transform {
public:
	Transform() = default;
	Transform(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation);
	~Transform() = default;
	Transform(const Transform&) = default;
	Transform& operator=(const Transform&) = default;
	Transform(Transform&&) = default;
	Transform& operator=(Transform&&) = default;

	XMFLOAT3 m_scale{ 1.0f, 1.0f, 1.0f };
	XMFLOAT3 m_rotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 m_position{ 0.0f, 0.0f, 0.0f };

	/*
	 * …Ë÷√Modelæÿ’Û
	 */
	static XMFLOAT4X4 GetModelMatrix(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation);
	static XMMATRIX GetModelMatrixXM(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation);
	static XMFLOAT4X4 GetModelMatrixInverse(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation);
	static XMMATRIX GetModelMatrixXMInverse(const XMFLOAT3& scale, const XMFLOAT3& rotation, const XMFLOAT3& translation);

	XMFLOAT3 GetUpAxis() const;
	XMVECTOR GetUpAxisXM() const;
	XMFLOAT3 GetRightAxis() const;
	XMVECTOR GetRightAxisXM() const;
	XMFLOAT3 GetForwardAxis() const;
	XMVECTOR GetForwardAxisXM() const;

	void LookAt(const XMFLOAT3& target, const XMFLOAT3& up);
	void Translate(const XMFLOAT3& dir, float steps);
	void Rotate(const XMFLOAT3& point, const XMFLOAT3& axis, float radians); // –˝◊™À≥–Ú «Z-X-Y
private:
	XMFLOAT3 GetEulerAnglesFromRotationMatrix(const XMFLOAT4X4& rotationMat);
};

