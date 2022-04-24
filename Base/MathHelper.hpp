#pragma once

#include <DirectXMath.h>
#include <random>
#include <windows.h>
#include <algorithm>

using namespace DirectX;

namespace Register
{
template <typename T>
struct Register : std::false_type{};
template <>
struct Register<XMFLOAT2> : std::true_type{};
template <>
struct Register<XMFLOAT3> : std::true_type{};
template <>
struct Register<XMFLOAT4> : std::true_type{};
}

namespace MathHelper
{
class MathHelper {
public:
	static float Rand()
	{
		static thread_local std::mt19937 generator;
		const std::uniform_int_distribution<int> distribution(0, RAND_MAX);
		return static_cast<float>(distribution(generator)) / static_cast<float>(RAND_MAX);
	}
#if _HAS_CXX17
	template <typename T, std::enable_if_t<!std::is_integral_v<T>, int> = 0>
	static T Rand(T a, T b)
	{
		return a + Rand() * (b - a);
	}
	template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static T Rand(T a, T b)
	{
		static thread_local std::mt19937 generator;
		const std::uniform_int_distribution<T> distribution(a, b);
		return distribution(generator);
	}
	template <typename T, std::enable_if_t<Register::Register<T>::value, int> = 0>
	static constexpr bool Compare(const T& a, const T& b)
	{
		XMVECTOR axm, bxm;
		if constexpr (std::is_same_v<T, XMFLOAT2>)
		{
			axm = XMLoadFloat2(&a);
			bxm = XMLoadFloat2(&b);
		} else if constexpr (std::is_same_v<T, XMFLOAT3>)
		{
			axm = XMLoadFloat3(&a);
			bxm = XMLoadFloat3(&b);
		} else if constexpr (std::is_same_v<T, XMFLOAT4>)
		{
			axm = XMLoadFloat4(&a);
			bxm = XMLoadFloat4(&b);
		}
		return XMVector4Equal(axm, bxm);
	}
	template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static T constexpr sqrtNewtonRaphson(T x, T curr, T prev)
	{
		return curr == prev
			? curr
			: sqrtNewtonRaphson(x, static_cast<T>(0.5) * (curr + x / curr), curr);
	}

	template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static T constexpr sqrt(T x)
	{
		return x >= 0 && x < std::numeric_limits<T>::infinity()
			? sqrtNewtonRaphson(x, x, static_cast<T>(0.0))
			: std::numeric_limits<T>::quiet_NaN();
	}
#else 
	template <typename T, typename std::enable_if<!std::is_integral<T>::value, int>::type = 0>
	static T Rand(T a, T b)
	{
		return a + Rand() * (b - a);
	}
	template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
	static T Rand(T a, T b)
	{
		static thread_local std::mt19937 generator;
		const std::uniform_int_distribution<T> distribution(a, b);
		return distribution(generator);
	}
	template <typename T, std::enable_if<Register::vaild<T>, int>::type = 0>
	static bool Compare(const T& a, const T& b)
	{
		XMVECTOR axm, bxm;
		if constexpr (std::is_same<T, XMFLOAT2>::value)
		{
			axm = XMLoadFloat2(&a);
			bxm = XMLoadFloat2(&b);
		} else if constexpr (std::is_same<T, XMFLOAT3>::value)
		{
			axm = XMLoadFloat3(&a);
			bxm = XMLoadFloat3(&b);
		} else if constexpr (std::is_same< T, XMFLOAT4>::value)
		{
			axm = XMLoadFloat4(&a);
			bxm = XMLoadFloat4(&b);
		}
		return XMVector4Equal(axm, bxm);
	}

	template <typename T, std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
	static T constexpr sqrtNewtonRaphson(T x, T curr, T prev)
	{
		return curr == prev
			? curr
			: sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
	}

	template <typename T, std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
	static T constexpr sqrt(T x)
	{
		return x >= 0 && x < std::numeric_limits<T>::infinity()
			? sqrtNewtonRaphson(x, x, 0)
			: std::numeric_limits<T>::quiet_NaN();
	}
#endif
	template<typename T>
	static T lerp(const T& a, const T& b, float t)
	{
		return a + (b - a) * t;
	}
	static XMVECTOR sphericalToCartesian(float radius, float theta, float phi)
	{
		return XMVectorSet(
			radius * std::cosf(theta) * std::sinf(phi),
			radius * std::cosf(phi),
			radius * std::sinf(theta) * std::sinf(phi),
			1.0f);
	}
	static XMFLOAT4X4 identity4x4()
	{
		static XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
		return I;
	}
	static XMFLOAT4X4A identity4x4A()
	{
		static XMFLOAT4X4A I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
		return I;
	}
	static constexpr float LowDiscrepancySequence(UINT index, UINT base)
	{
		float res = 0.0f;
		float fraction = 1.0f / static_cast<float>(base);
		while (index > 0)
		{
			res += static_cast<float>(index % base) * fraction;
			index /= base;
			fraction /= static_cast<float>(base);
		}
		return res;
	}
	template<UINT T>
	static constexpr std::array<XMFLOAT2, T> HaltonSequence()
	{
		auto ans;
		for (UINT i = 0; i < T; ++i)
		{
			ans[i] = XMFLOAT2{ LowDiscrepancySequence(i & 1023, 2), LowDiscrepancySequence(i & 1023, 3) };
		}
		return ans;
	} 
};
}

