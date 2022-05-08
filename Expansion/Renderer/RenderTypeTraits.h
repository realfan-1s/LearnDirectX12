#pragma once

#include <type_traits>
#include <Windows.h>
#include "Renderer/DeferShading.h"

namespace Renderer
{
template <typename T>
struct RendererConstant : public std::integral_constant<UINT, 0> {
};
template <>
struct RendererConstant<DeferShading> : public std::integral_constant<UINT, 1> {};
}
