#pragma once
#include "IRenderer.h"

namespace Renderer
{
class ForwardPlus final : public IRenderer
{
public:
	ForwardPlus(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format);
	ForwardPlus(const ForwardPlus&) = delete;
	ForwardPlus& operator=(const ForwardPlus&) = delete;
	ForwardPlus(ForwardPlus&&) = default;
	ForwardPlus& operator=(ForwardPlus&&) = default;
	~ForwardPlus() override = default;

private:
};
}

