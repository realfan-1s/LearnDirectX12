#include "Camera.h"
#include <DirectXCollision.h>

using namespace DirectX;

Camera::Camera() : m_transform(std::make_shared<Transform>())
{
}

void Camera::SetJitter(const XMFLOAT2& curr)
{
	const auto newProj = std::move(XMMatrixSet(
		m_nonjitteredProj._11,			m_nonjitteredProj._12,			m_nonjitteredProj._13, m_nonjitteredProj._14,
		m_nonjitteredProj._21,			m_nonjitteredProj._22,			m_nonjitteredProj._23, m_nonjitteredProj._24,
		m_nonjitteredProj._31 + curr.x, m_nonjitteredProj._32 + curr.y, m_nonjitteredProj._33, m_nonjitteredProj._34,
		m_nonjitteredProj._41,			m_nonjitteredProj._42,			m_nonjitteredProj._43, m_nonjitteredProj._44));
	XMStoreFloat4x4(&m_proj, newProj);
}

const XMFLOAT3& Camera::GetCurrPos() const
{
	return m_transform->m_position;
}

XMVECTOR Camera::GetCurrPosXM() const
{
	return XMLoadFloat3(&m_transform->m_position);
}

const XMFLOAT3& Camera::GetCurrRotation() const
{
	return m_transform->m_rotation;
}

XMVECTOR Camera::GetCurrRotationXM() const
{
	return XMLoadFloat3(&m_transform->m_rotation);
}

XMFLOAT4X4 Camera::GetCurrModel() const
{
	return Transform::GetModelMatrix(m_transform->m_scale, m_transform->m_rotation, m_transform->m_position);
}

XMMATRIX Camera::GetCurrModelXM() const
{
	return m_transform->GetModelMatrixXM(m_transform->m_scale, m_transform->m_rotation, m_transform->m_position);
}

const XMFLOAT4X4& Camera::GetCurrView() const
{
	return m_view;
}

XMMATRIX Camera::GetCurrViewXM() const
{
	return DirectX::XMLoadFloat4x4(&m_view);
}

const XMFLOAT4X4& Camera::GetCurrProj() const
{
	return m_proj;
}

XMMATRIX Camera::GetCurrProjXM() const
{
	return DirectX::XMLoadFloat4x4(&GetCurrProj());
}

const XMFLOAT4X4& Camera::GetNonjitteredProj() const
{
	return m_nonjitteredProj;
}

XMMATRIX Camera::GetNonjitteredProjXM() const
{
	return XMLoadFloat4x4(&GetNonjitteredProj());
}

const D3D12_VIEWPORT& Camera::GetViewPort() const
{
	return m_viewport;
}

XMFLOAT3 Camera::GetCurrRight() const
{
	return m_transform->GetRightAxis();
}

XMVECTOR Camera::GetCurrRightXM() const
{
	return m_transform->GetRightAxisXM();
}

XMFLOAT3 Camera::GetCurrUp() const
{
	return m_transform->GetUpAxis();
}

XMVECTOR Camera::GetCurrUpXM() const
{
	return m_transform->GetUpAxisXM();
}

XMFLOAT3 Camera::GetCurrForward() const
{
	return m_transform->GetForwardAxis();
}

XMVECTOR Camera::GetCurrForwardXM() const
{
	return m_transform->GetForwardAxisXM();
}

XMFLOAT4X4 Camera::GetCurrVP() const
{
	XMFLOAT4X4 currVP;
	DirectX::XMStoreFloat4x4(&currVP, GetCurrVPXM());
	return currVP;
}

XMMATRIX Camera::GetCurrVPXM() const
{
	return XMMatrixMultiply(GetCurrViewXM(), GetCurrProjXM());
}

XMFLOAT4X4 Camera::GetNonjitteredCurrVP() const
{
	XMFLOAT4X4 currVP;
	DirectX::XMStoreFloat4x4(&currVP, GetNonjitteredCurrVPXM());
	return currVP;
}

XMMATRIX Camera::GetNonjitteredCurrVPXM() const
{
	return XMMatrixMultiply(GetCurrViewXM(), GetNonjitteredProjXM());
}

const XMFLOAT4X4& Camera::GetNonJitteredPreviousVP() const
{
	return m_previousVP;
}

XMMATRIX Camera::GetNonJitteredPreviousVPXM() const
{
	return DirectX::XMLoadFloat4x4(&m_previousVP);
}

XMMATRIX Camera::GetInvProjXM() const {
	return XMMatrixInverse(nullptr, GetCurrProjXM());
}

XMFLOAT4X4 Camera::GetInvProj() const {
	XMFLOAT4X4 ans;
	XMStoreFloat4x4(&ans, GetInvProjXM());
	return ans;
}

XMMATRIX Camera::GetInvViewXM() const {
	return XMMatrixInverse(nullptr, GetCurrViewXM());
}

XMFLOAT4X4 Camera::GetInvView() const {
	XMFLOAT4X4 ans;
	XMStoreFloat4x4(&ans, GetInvViewXM());
	return ans;
}

XMMATRIX Camera::GetInvVPXM() const {
	return XMMatrixInverse(nullptr , GetCurrVPXM());
}

XMFLOAT4X4 Camera::GetInvVP() const {
	XMFLOAT4X4 ans;
	XMStoreFloat4x4(&ans, GetInvVPXM());
	return ans;
}

XMMATRIX Camera::GetViewPortRayXM() const {
	float halfHeight = 0.5f * m_nearWndHeight;
	float halfWidth = halfHeight * m_aspect;
	auto toTop = GetCurrUpXM() * halfHeight;
	auto toRight = GetCurrRightXM() * halfWidth;
	auto topLeft = GetCurrForwardXM() * m_nearPlane + toTop - toRight;
	//auto scale = XMVector3Length(topLeft) / m_nearPlane;
	//topLeft = XMVector3Normalize(topLeft);
	//topLeft *= scale;
	auto topRight = GetCurrForwardXM() * m_nearPlane + toTop + toRight;
	//topRight = XMVector3Normalize(topRight);
	//topRight *= scale;
	auto bottomRight = GetCurrForwardXM() * m_nearPlane - toTop + toRight;
	//bottomRight = XMVector3Normalize(bottomRight);
	//bottomRight *= scale;
	auto bottomLeft = GetCurrForwardXM() * m_nearPlane - toTop - toRight;
	//bottomLeft = XMVector3Normalize(bottomLeft);
	//bottomLeft *= scale;
	return  { topLeft, topRight, bottomRight, bottomLeft };
}

XMFLOAT4X4 Camera::GetViewPortRay() const {
	XMFLOAT4X4 ans;
	XMStoreFloat4x4(&ans, GetViewPortRayXM());
	return ans;
}

void Camera::GetFrustumPlanes(XMFLOAT4* planes) const
{
	const BoundingFrustum projFrustum(GetCurrProjXM());
	XMVECTOR frustumPlanesXM[6];
	projFrustum.GetPlanes(&frustumPlanesXM[0], &frustumPlanesXM[1], &frustumPlanesXM[2],
		&frustumPlanesXM[3], &frustumPlanesXM[4], &frustumPlanesXM[5]);
	XMFLOAT4 frustumPlanes[6];
	for (int i = 0; i < 6; ++i)
	{
		XMStoreFloat4(&frustumPlanes[i], frustumPlanesXM[i]);
	}
	memcpy(planes, &frustumPlanes, sizeof(XMFLOAT4) * 6);
}

void Camera::SetFrustum(float fov, float aspect, float nearZ, float farZ)
{
	m_fov = fov;
	m_aspect = aspect;
	m_nearPlane = nearZ;
	m_farPlane = farZ;

	m_nearWndHeight = 2.0f * m_nearPlane * std::tanf(0.5f * m_fov);
	m_farWndHeight = 2.0f * m_farPlane * std::tanf(0.5f * m_fov);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearPlane, m_farPlane);
	DirectX::XMStoreFloat4x4(&m_nonjitteredProj, proj);
	m_proj = m_nonjitteredProj;
}

void Camera::SetFrustumReverseZ(float fov, float aspect, float nearZ, float farZ) {
	m_fov = fov;
	m_aspect = aspect;
	m_nearPlane = farZ;
	m_farPlane = nearZ;

	m_nearWndHeight = 2.0f * m_nearPlane * std::tanf(0.5f * m_fov);
	m_farWndHeight = 2.0f * m_farPlane * std::tanf(0.5f * m_fov);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearPlane, m_farPlane);
	DirectX::XMStoreFloat4x4(&m_nonjitteredProj, proj);
	m_proj = m_nonjitteredProj;
}

void Camera::SetViewPort(const D3D12_VIEWPORT& viewport)
{
	m_viewport = viewport;
}

void Camera::SetViewPort(float topLeftX, float topLeftY, float width, float height, float minDepth, float maxDepth)
{
	m_viewport.TopLeftX = topLeftX;
	m_viewport.TopLeftY = topLeftY;
	m_viewport.Width = width;
	m_viewport.Height = height;
	m_viewport.MinDepth = minDepth;
	m_viewport.MaxDepth = maxDepth;
}

void Camera::LookAt(const XMFLOAT3& center, const XMFLOAT3& target, const XMFLOAT3& up)
{
	m_transform->m_position = center;
	m_transform->LookAt(target, up);
	isMoved = true;
}

FirstPersonCamera::FirstPersonCamera() : Camera()
{
}

FirstPersonCamera::~FirstPersonCamera() = default;

void FirstPersonCamera::Update()
{
	m_previousVP = std::move(GetNonjitteredCurrVP());
	if (isMoved)
	{
		XMVECTOR right = std::move(GetCurrRightXM());
		XMVECTOR up = std::move(GetCurrUpXM());
		XMVECTOR forward = std::move(GetCurrForwardXM());
		XMVECTOR pos = std::move(GetCurrPosXM());

		/*
		 * ????????????
		 */
		forward = std::move(DirectX::XMVector3Normalize(forward));
		up = std::move(DirectX::XMVector3Normalize(std::move(DirectX::XMVector3Cross(forward, right))));
		right = std::move(DirectX::XMVector3Normalize(std::move(DirectX::XMVector3Cross(up, forward))));

		// ????????????
		float x = -DirectX::XMVectorGetX(std::move(DirectX::XMVector3Dot(pos, right)));
		float y = -DirectX::XMVectorGetX(std::move(DirectX::XMVector3Dot(pos, up)));
		float z = -DirectX::XMVectorGetX(std::move(DirectX::XMVector3Dot(pos, forward)));

		XMFLOAT3 r, u, f;
		XMStoreFloat3(&r, right);
		XMStoreFloat3(&u, up);
		XMStoreFloat3(&f, forward);
		/*
		 * ????????????
		 */
		m_view(0, 0) = r.x;
		m_view(1, 0) = r.y;
		m_view(2, 0) = r.z;
		m_view(3, 0) = x;
		m_view(0, 1) = u.x;
		m_view(1, 1) = u.y;
		m_view(2, 1) = u.z;
		m_view(3, 1) = y;
		m_view(0, 2) = f.x;
		m_view(1, 2) = f.y;
		m_view(2, 2) = f.z;
		m_view(3, 2) = z;
		m_view(0, 3) = 0.0f;
		m_view(1, 3) = 0.0f;
		m_view(2, 3) = 0.0f;
		m_view(3, 3) = 1.0f;

		isMoved = false;
	}
}

void FirstPersonCamera::Strafe(float d)
{
	m_transform->Translate(GetCurrRight(), d);
	isMoved = true;
}

void FirstPersonCamera::MoveForward(float d)
{
	m_transform->Translate(GetCurrForward(), d);
	isMoved = true;
}

void FirstPersonCamera::Pitch(float radians)
{
	m_transform->m_rotation.x += radians;
	std::clamp(m_transform->m_rotation.x, -XM_PI * 7.0f / 18.0f, XM_PI * 7.0f / 18.0f);
	isMoved = true;
}

void FirstPersonCamera::Rotate(float radians)
{
	float temp = m_transform->m_rotation.y;
	m_transform->m_rotation.y = XMScalarModAngle(temp + radians);
	isMoved = true;
}
