#include "rndr/projections.h"

Rndr::Matrix4x4f Rndr::PerspectiveOpenGL(f32 vertical_fov, f32 aspect_ratio, f32 near, f32 far)
{
    assert(aspect_ratio != 0);

    const f32 inv_fov_y = 1.0f / Math::Tan(Math::Radians(vertical_fov) / 2);
    const f32 inv_aspect_ratio = 1 / aspect_ratio;

    Matrix4x4f mat(1);
    mat.elements[0][0] = inv_fov_y * inv_aspect_ratio;
    mat.elements[1][1] = inv_fov_y;
    mat.elements[2][2] = -(far + near) / (far - near);
    mat.elements[2][3] = -(2 * far * near) / (far - near);
    mat.elements[3][2] = -1;
    mat.elements[3][3] = 0;
    return mat;
}

Rndr::Matrix4x4f Rndr::PerspectiveVulkan(f32 vertical_fov, f32 aspect_ratio, f32 near, f32 far)
{
    assert(aspect_ratio != 0);

    const f32 inv_fov_y = 1.0f / Math::Tan(Math::Radians(vertical_fov) / 2);
    const f32 inv_aspect_ratio = 1 / aspect_ratio;

    Matrix4x4f mat(1);
    mat.elements[0][0] = inv_fov_y * inv_aspect_ratio;
    mat.elements[1][1] = -inv_fov_y;
    mat.elements[2][2] = -far / (far - near);
    mat.elements[2][3] = -(far * near) / (far - near);
    mat.elements[3][2] = -1;
    mat.elements[3][3] = 0;
    return mat;
}