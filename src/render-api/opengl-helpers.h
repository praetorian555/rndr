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
GLenum FromImageInfoToTarget(ImageType image_type, bool use_mips);
GLint FromImageFilterToMinFilter(ImageFilter min_filter, ImageFilter mip_filter);
GLint FromImageFilterToOpenGL(ImageFilter filter);
GLint FromImageAddressModeToOpenGL(ImageAddressMode wrap);
GLenum FromPixelFormatToInternalFormat(PixelFormat format);
GLenum FromPixelFormatToFormat(PixelFormat format);
GLenum FromPixelFormatToDataType(PixelFormat format);


} // namespace Rndr

#endif  // RNDR_OPENGL
