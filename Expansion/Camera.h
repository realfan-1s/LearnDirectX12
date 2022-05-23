#pragma once

#include <memory>
#include "Transform.h"
#include <d3d12.h>
#include "MathHelper.hpp"

class Camera {
public:
	enum CameraType
	{
		first,
	};
	Camera();
	virtual ~Camera() = default;
	Camera(const Camera&) = default;
	Camera& operator=(const Camera&) = default;
	Camera(Camera&&) = default;
	Camera& operator=(Camera&&) = default;

	virtual void Update() = 0;

	const XMFLOAT3&	GetCurrPos() const;
	XMVECTOR		GetCurrPosXM() const;
	const XMFLOAT3&	GetCurrRotation() const;
	XMVECTOR		GetCurrRotationXM() const;

	/*
	 *  获取MVP矩阵
	 */
	XMFLOAT4X4			GetCurrModel() const;
	XMMATRIX			GetCurrModelXM() const;
	const XMFLOAT4X4& 	GetCurrView() const;
	XMMATRIX			GetCurrViewXM() const;
	const XMFLOAT4X4&	GetCurrProj() const;
	XMMATRIX			GetCurrProjXM() const;

	/*
	 * 获取视口
	 */
	const D3D12_VIEWPORT& GetViewPort() const;

	/*
	 * 获取相机的right、Up、Forward
	 */
	XMFLOAT3			GetCurrRight() const;
	XMVECTOR			GetCurrRightXM() const;
	XMFLOAT3			GetCurrUp() const;
	XMVECTOR			GetCurrUpXM() const;
	XMFLOAT3			GetCurrForward() const;
	XMVECTOR			GetCurrForwardXM() const;

	XMFLOAT4X4			GetCurrVP() const;
	XMMATRIX			GetCurrVPXM() const;
	const XMFLOAT4X4&	GetPreviousVP() const;
	XMMATRIX			GetPreviousVPXM() const;
	XMMATRIX			GetInvProjXM() const;
	XMFLOAT4X4			GetInvProj() const;
	XMMATRIX			GetInvViewXM() const;
	XMFLOAT4X4			GetInvView() const;
	XMMATRIX			GetInvVPXM() const;
	XMFLOAT4X4			GetInvVP() const;
	XMMATRIX			GetViewPortRayXM() const;
	XMFLOAT4X4			GetViewPortRay() const;

	void SetJitter(const XMFLOAT2& prev, const XMFLOAT2& curr);
	void SetFrustum(float fov, float aspect, float nearZ, float farZ);
	void SetFrustumReverseZ(float fov, float aspect, float nearZ, float farZ);
	void SetViewPort(const D3D12_VIEWPORT& viewport);
	void SetViewPort(float topLeftX, float topLeftY, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
	void LookAt(const XMFLOAT3& center, const XMFLOAT3& target, const XMFLOAT3& up);
	// 朝向左右移动,正方向为向左，负方向为向右
	virtual void Strafe(float d) = 0;
	// 前进，正方向为向前，负方向为向后
	virtual void MoveForward(float d) = 0;
	// 上下观察，正radians向上观察，负radians向下观察
	virtual void Pitch(float radians) = 0;
	// 左右观察，正radians向左观察，负radians向右观察
	virtual void Rotate(float radians) = 0;

	/*
	 * projection 相关参数
	 */
	float						m_nearPlane{ 0 };
	float						m_farPlane{ 0 };
	float						m_fov{ 0 };
	float						m_aspect{ 0 };
	float						m_nearWndHeight{ 0 };
	float						m_farWndHeight{ 0 };

protected:
	std::shared_ptr<Transform>	m_transform{ nullptr };
	D3D12_VIEWPORT				m_viewport{};
	XMFLOAT4X4					m_previousVP{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4					m_proj{ MathHelper::MathHelper::identity4x4() };
	XMFLOAT4X4					m_view{ MathHelper::MathHelper::identity4x4() };
	bool						isMoved{ true };
};

class FirstPersonCamera : public Camera
{
public:
	FirstPersonCamera();
	~FirstPersonCamera() override;
	FirstPersonCamera(const FirstPersonCamera&) = default;
	FirstPersonCamera& operator=(const FirstPersonCamera&) = default;
	FirstPersonCamera(FirstPersonCamera&&) = default;
	FirstPersonCamera& operator=(FirstPersonCamera&&) = default;

	void Update() override;

	// 朝向左右移动
	void Strafe(float d) override;
	// 前进
	void MoveForward(float d) override;
	// 上下观察，正radians向上观察，负radians向下观察
	void Pitch(float radians) override;
	// 左右观察，正radians向左观察，负radians向右观察
	void Rotate(float radians) override;
};