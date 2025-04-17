#pragma once

#include "rndr/math.h"
#include "rndr/types.h"

namespace Rndr
{

/**
 * Create a orthographic projection matrix that assumes that your view space is in a right-handed
 * coordinate system, and we are switching to a left-handed coordinate system where Z maps between -1
 * and 1. This is setup needed for OpenGL.
 *
 * @param left Position of the left clipping plane.
 * @param right Position of the right clipping plane.
 * @param bottom Position of the bottom clipping plane.
 * @param top Position of the top clipping plane.
 * @param near Position of the near clipping plane relative to the camera. Should be positive.
 * @param far Position of the far clipping plane relative to the camera. Should be positive.
 * @return Returns 4x4 orthographic projection matrix.
 *
 * Example:
 *      Matrix4x4f projection = OrthographicOpenGL(-500.0f, 500.0f, -100.0f, 100.0f, 0.1f, 100.0f);
 */
Matrix4x4f OrthographicOpenGL(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);

/**
 * Create a perspective projection matrix that assumes that your view space is in a right-handed
 * coordinate system, and we are switching to a left-handed coordinate system where Z maps between -1
 * and 1. This is setup needed for OpenGL.
 *
 * @param vertical_fov Field of view angle in y direction. In degrees.
 * @param aspect_ratio Ratio used to calculate a field of view angle in x direction. Defined as a ratio
 * between width (x) and height (y).
 * @param near Distance from a viewer to the near clipping plane. Always positive.
 * @param far Distance from a viewer to the far clipping plane. Always positive.
 * @return Returns 4x4 perspective projection matrix.
 *
 * Example:
 *      Matrix4x4f projection = PerspectiveOpenGL(60.0f, width / height, 0.1f, 100.0f);
 */
Matrix4x4f PerspectiveOpenGL(f32 vertical_fov, f32 aspect_ratio, f32 near, f32 far);

/**
 * Create a perspective projection matrix that assumes that your view space is in a right-handed
 * coordinate system. We rotated it around x-axis for 180 degrees so that we now look down the
 * positive Z axis (before applying perspective projection) and Z maps between 0 and 1.
 *
 * @param vertical_fov Field of view angle in y direction. In degrees.
 * @param aspect_ratio Ratio used to calculate a field of view angle in x direction. Defined as a ratio
 * between width (x) and height (y).
 * @param near Distance from a viewer to the near clipping plane. Always positive.
 * @param far Distance from a viewer to the far clipping plane. Always positive.
 * @return Returns 4x4 perspective projection matrix.
 *
 * Example:
 *      Matrix4x4f projection = PerspectiveOpenGL(60.0f, width / height, 0.1f, 100.0f);
 */
Matrix4x4f PerspectiveVulkan(f32 vertical_fov, f32 aspect_ratio, f32 near, f32 far);
}
