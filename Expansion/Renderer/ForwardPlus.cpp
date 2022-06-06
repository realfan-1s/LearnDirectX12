#include "ForwardPlus.h"

using namespace Renderer;

ForwardPlus::ForwardPlus(ID3D12Device* _device, UINT _width, UINT _height, DXGI_FORMAT _format)
: IRenderer(_device, _width, _height, _format)
{
}
