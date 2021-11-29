#include "rndr/render/pipeline.h"

namespace rndr
{

rndr::VertexShader g_DefaultVertexShader{[](const PerVertexInfo& Info) { return Info.Position; }};
rndr::PixelShader g_DefaultPixelShader{[](const PerPixelInfo& Info) { return Color(0, 0, 0, 0); }};

}  // namespace rndr