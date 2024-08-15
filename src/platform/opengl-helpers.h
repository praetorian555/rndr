#pragma once

#include "rndr/definitions.h"
#include "rndr/graphics-types.h"
#include "rndr/platform/opengl-forward-def.h"

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
GLenum FromImageInfoToTarget(TextureType image_type, bool is_multi_sample);
GLint FromMinAndMipFiltersToOpenGL(ImageFilter min_filter, ImageFilter mip_filter);
GLint FromImageFilterToOpenGL(ImageFilter filter);
GLint FromImageAddressModeToOpenGL(ImageAddressMode wrap);
GLint FromImageAccessToOpenGL(TextureAccess access);
GLenum FromPixelFormatToInternalFormat(PixelFormat format);
GLenum FromPixelFormatToExternalFormat(PixelFormat format);
GLenum FromPixelFormatToDataType(PixelFormat format);
GLenum FromPixelFormatToShouldNormalizeData(PixelFormat format);
bool IsPixelFormatInteger(PixelFormat format);
GLenum FromPrimitiveTopologyToOpenGL(PrimitiveTopology topology);
GLenum FromIndexSizeToOpenGL(int64_t index_size);

Opal::StringUtf8 FromOpenGLDataTypeToString(GLenum value);
Opal::StringUtf8 FromOpenGLUsageToString(GLenum value);

}  // namespace Rndr

#define RNDR_ASSERT_OPENGL()          \
    if (glGetError() != GL_NO_ERROR)  \
    {                                 \
        RNDR_HALT("OpenGL failure!"); \
    }

#define RNDR_GL_VERIFY(message, do_if_fails)                            \
    {                                                                   \
        const GLuint gl_err = glGetError();                             \
        if (gl_err != GL_NO_ERROR)                                      \
        {                                                               \
            RNDR_LOG_ERROR("OpenGL error: 0x%x - %s", gl_err, message); \
            do_if_fails;                                                \
            return ErrorCode::GraphicsAPIError;                         \
        }                                                               \
    }

#endif  // RNDR_OPENGL
