#include "rndr/core/transform.h"

namespace rndr
{

Transform::Transform(const real mat[4][4]) : m_Matrix(mat), m_MatrixInverse(m_Matrix.Inverse()) {}

Transform::Transform(const Matrix4x4& mat) : m_Matrix(mat), m_MatrixInverse(m_Matrix.Inverse()) {}

Transform::Transform(const Matrix4x4& mat, const Matrix4x4& invMat) : m_Matrix(mat), m_MatrixInverse(invMat) {}

Transform Inverse(const Transform& t)
{
    return Transform(t.m_MatrixInverse, t.m_Matrix);
}

Transform Transpose(const Transform& t)
{
    return Transform(t.m_Matrix.Transpose(), t.m_MatrixInverse.Transpose());
}

bool Transform::operator==(const Transform& other) const
{
    return m_Matrix == other.m_Matrix && other.m_MatrixInverse == other.m_MatrixInverse;
}

bool Transform::operator!=(const Transform& other) const
{
    return !(*this == other);
}

bool Transform::IsIdentity() const
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (i == j)
            {
                if (m_Matrix.m[i][j] != 1.0f)
                {
                    return false;
                }
            }
            else
            {
                if (m_Matrix.m[i][j] != 0.0f)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool Transform::HasScale() const
{
    real la2 = (*this)(Vector3r(1, 0, 0)).LengthSquared();
    real lb2 = (*this)(Vector3r(0, 1, 0)).LengthSquared();
    real lc2 = (*this)(Vector3r(0, 0, 1)).LengthSquared();
#define NOT_ONE(x) ((x) < .999f || (x) > 1.001f)
    return (NOT_ONE(la2) || NOT_ONE(lb2) || NOT_ONE(lc2));
#undef NOT_ONE
}

Transform Transform::operator*(const Transform& other) const
{
    Matrix4x4 m = Multiply(m_Matrix, other.m_Matrix);
    Matrix4x4 minv = Multiply(other.m_MatrixInverse, m_MatrixInverse);

    return Transform(m, minv);
}

bool Transform::SwapsHandedness() const
{
    real det = m_Matrix.m[0][0] * (m_Matrix.m[1][1] * m_Matrix.m[2][2] - m_Matrix.m[1][2] * m_Matrix.m[2][1]) -
               m_Matrix.m[0][1] * (m_Matrix.m[1][0] * m_Matrix.m[2][2] - m_Matrix.m[1][2] * m_Matrix.m[2][0]) +
               m_Matrix.m[0][2] * (m_Matrix.m[1][0] * m_Matrix.m[2][1] - m_Matrix.m[1][1] * m_Matrix.m[2][0]);
    return det < 0;
}

Transform Translate(const Vector3r& delta)
{
    // clang-format off
    Matrix4x4 m(
        1, 0, 0, delta.X,
        0, 1, 0, delta.Y,
        0, 0, 1, delta.Z,
        0, 0, 0,       1
    );

    Matrix4x4 minv(
        1, 0, 0, -delta.X,
        0, 1, 0, -delta.Y,
        0, 0, 1, -delta.Z,
        0, 0, 0,        1
    );
    // clang-format on

    return Transform(m, minv);
}

Transform Scale(real x, real y, real z)
{
    // clang-format off
    Matrix4x4 m(
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, z, 0,
        0, 0, 0, 1
    );

    Matrix4x4 minv(
        1 / x,     0,     0, 0,
            0, 1 / y,     0, 0,
            0,     0, 1 / z, 0,
            0,     0,     0, 1
    );
    // clang-format on

    return Transform(m, minv);
}

Transform RotateX(real theta)
{
    real sinTheta = std::sin(Radians(theta));
    real cosTheta = std::cos(Radians(theta));

    // clang-format off
    Matrix4x4 m(
        1,        0,         0, 0, 
        0, cosTheta, -sinTheta, 0,
        0, sinTheta,  cosTheta, 0,
        0,        0,         0, 1);
    // clang-format on

    return Transform(m, m.Transpose());
}

Transform RotateY(real theta)
{
    real sinTheta = std::sin(Radians(theta));
    real cosTheta = std::cos(Radians(theta));

    // clang-format off
    Matrix4x4 m(
         cosTheta, 0, sinTheta, 0, 
                0, 1,        0, 0,
        -sinTheta, 0, cosTheta, 0,
                0, 0,        0, 1);
    // clang-format on

    return Transform(m, m.Transpose());
}

Transform RotateZ(real theta)
{
    real sinTheta = std::sin(Radians(theta));
    real cosTheta = std::cos(Radians(theta));

    // clang-format off
    Matrix4x4 m(
        cosTheta, -sinTheta, 0, 0, 
        sinTheta,  cosTheta, 0, 0,
               0,         0, 1, 0,
               0,         0, 0, 1);
    // clang-format on

    return Transform(m, m.Transpose());
}

Transform Rotate(real theta, const Vector3r& axis)
{
    Vector3r a = Normalize(axis);
    real sinTheta = std::sin(Radians(theta));
    real cosTheta = std::cos(Radians(theta));
    Matrix4x4 m;

    // Compute rotation of first basis vector
    m.m[0][0] = a.X * a.X + (1 - a.X * a.X) * cosTheta;
    m.m[0][1] = a.X * a.Y * (1 - cosTheta) - a.Z * sinTheta;
    m.m[0][2] = a.X * a.Z * (1 - cosTheta) + a.Y * sinTheta;
    m.m[0][3] = 0;

    // Compute rotations of second and third basis vectors
    m.m[1][0] = a.X * a.Y * (1 - cosTheta) + a.Z * sinTheta;
    m.m[1][1] = a.Y * a.Y + (1 - a.Y * a.Y) * cosTheta;
    m.m[1][2] = a.Y * a.Z * (1 - cosTheta) - a.X * sinTheta;
    m.m[1][3] = 0;

    m.m[2][0] = a.X * a.Z * (1 - cosTheta) - a.Y * sinTheta;
    m.m[2][1] = a.Y * a.Z * (1 - cosTheta) + a.X * sinTheta;
    m.m[2][2] = a.Z * a.Z + (1 - a.Z * a.Z) * cosTheta;
    m.m[2][3] = 0;

    return Transform(m, m.Transpose());
}

Transform Rotate(Rotator Rotator)
{
    return RotateY(Rotator.Yaw) * RotateZ(Rotator.Pitch) * RotateX(Rotator.Roll);
}

Transform LookAt(const Point3r& pos, const Point3r& look, const Vector3r& up)
{
    Matrix4x4 cameraToWorld;

    cameraToWorld.m[0][3] = pos.X;
    cameraToWorld.m[1][3] = pos.Y;
    cameraToWorld.m[2][3] = pos.Z;
    cameraToWorld.m[3][3] = 1;

    // This is z-axis in left-handend system, if we used right-handed we would have to negate it
    Vector3r zAxis = Normalize(look - pos);
    Vector3r xAxis = Cross(Normalize(up), zAxis);
    Vector3r yAxis = Cross(zAxis, xAxis);

    cameraToWorld.m[0][0] = xAxis.X;
    cameraToWorld.m[1][0] = xAxis.Y;
    cameraToWorld.m[2][0] = xAxis.Z;
    cameraToWorld.m[3][0] = 0.0f;

    cameraToWorld.m[0][1] = yAxis.X;
    cameraToWorld.m[1][1] = yAxis.Y;
    cameraToWorld.m[2][1] = yAxis.Z;
    cameraToWorld.m[3][1] = 0.0f;

    cameraToWorld.m[0][2] = zAxis.X;
    cameraToWorld.m[1][2] = zAxis.Y;
    cameraToWorld.m[2][2] = zAxis.Z;
    cameraToWorld.m[3][2] = 0.0f;

    return Transform(cameraToWorld.Inverse(), cameraToWorld);
}

Bounds3r Transform::operator()(const Bounds3r& b) const
{
    // TODO(mkostic): Make this more efficient

    const Transform& M = *this;
    Bounds3r ret(M(Point3r(b.pMin.X, b.pMin.Y, b.pMin.Z)));
    ret = Union(ret, M(Point3r(b.pMax.X, b.pMin.Y, b.pMin.Z)));
    ret = Union(ret, M(Point3r(b.pMin.X, b.pMax.Y, b.pMin.Z)));
    ret = Union(ret, M(Point3r(b.pMin.X, b.pMin.Y, b.pMax.Z)));
    ret = Union(ret, M(Point3r(b.pMin.X, b.pMax.Y, b.pMax.Z)));
    ret = Union(ret, M(Point3r(b.pMax.X, b.pMax.Y, b.pMin.Z)));
    ret = Union(ret, M(Point3r(b.pMax.X, b.pMin.Y, b.pMax.Z)));
    ret = Union(ret, M(Point3r(b.pMax.X, b.pMax.Y, b.pMax.Z)));
    return ret;
}

}  // namespace rndr