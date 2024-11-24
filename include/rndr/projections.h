#pragma once

#include "rndr/math.h"
#include "rndr/types.h"

namespace Rndr
{

/**
 * Create a perspective projection matrix that assumes that your view space was in right-handed
 * coordinate system and we are switching to left-handed coordinate system where Z maps between -1
 * and 1 (OpenGL style).
 *
 * @param vertical_fov Field of view angle in y direction. In degrees.
 * @param aspect_ratio Ratio used to calculate field of view angle in x direction. Defined as ratio
 * between width (x) and height (y).
 * @param near Distance from a viewer to the near clipping plane. Always positive.
 * @param far Distance from a viewer to the far clipping plane. Always positive.
 */
Matrix4x4f PerspectiveOpenGL(f32 vertical_fov, f32 aspect_ratio, f32 near, f32 far);

/**
 * Create a perspective projection matrix that assumes that your view space was in right-handed
 * coordinate system and we rotated it around x axis for 180 degrees so that we now look down the
 * positive Z axis (before applying perspective projection) and Z maps between 0 and 1.
 *
 * @param vertical_fov Field of view angle in y direction. In degrees.
 * @param aspect_ratio Ratio used to calculate field of view angle in x direction. Defined as ratio
 * between width (x) and height (y).
 * @param near Distance from a viewer to the near clipping plane. Always positive.
 * @param far Distance from a viewer to the far clipping plane. Always positive.
 */
Matrix4x4f PerspectiveVulkan(f32 vertical_fov, f32 aspect_ratio, f32 near, f32 far);
}
