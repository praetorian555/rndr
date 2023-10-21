#pragma once

#include "rndr/core/base.h"
#include "rndr/core/forward-def-opengl.h"
#include "rndr/core/graphics-types.h"

#if RNDR_OPENGL

namespace Rndr
{

GLenum FromShaderTypeToOpenGL(ShaderType type);
GLenum FromUsageToOpenGL(Usage usage);
GLenum FromBufferTypeToOpenGL(BufferType type);
GLenum FromComparatorToOpenGL(Comparator comparator);
GLenum FromStencilOpToOpenGL(StencilOperation op);
GLenum FromBlendFactorToOpenGL(BlendFactor factor);
GLenum FromBlendOperationToOpenGL(BlendOperation op);
GLenum FromImageInfoToTarget(ImageType image_type, bool is_multi_sample);
GLint FromMinAndMipFiltersToOpenGL(ImageFilter min_filter, ImageFilter mip_filter);
GLint FromImageFilterToOpenGL(ImageFilter filter);
GLint FromImageAddressModeToOpenGL(ImageAddressMode wrap);
GLenum FromPixelFormatToInternalFormat(PixelFormat format);
GLenum FromPixelFormatToFormat(PixelFormat format);
GLenum FromPixelFormatToDataType(PixelFormat format);
GLenum FromPixelFormatToShouldNormalizeData(PixelFormat format);
GLenum FromPrimitiveTopologyToOpenGL(PrimitiveTopology topology);
GLenum FromIndexSizeToOpenGL(uint32_t index_size);

Rndr::String FromOpenGLDataTypeToString(GLenum value);
Rndr::String FromOpenGLUsageToString(GLenum value);

} // namespace Rndr

#endif  // RNDR_OPENGL
