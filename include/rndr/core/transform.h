#pragma once

#include "rndr/core/bounds3.h"
#include "rndr/core/math.h"

namespace rndr
{

class Transform
{
public:
    Transform() = default;
    Transform(const real mat[4][4]);
    Transform(const math::Matrix4x4& mat);
    Transform(const math::Matrix4x4& mat, const math::Matrix4x4& invMat);

    const math::Matrix4x4& GetMatrix() const { return m_Matrix; }
    const math::Matrix4x4& GetInverse() const { return m_MatrixInverse; }

    friend Transform Inverse(const Transform& t);
    friend Transform Transpose(const Transform& t);

    bool operator==(const Transform& other) const;
    bool operator!=(const Transform& other) const;

    bool IsIdentity() const;

    bool HasScale() const;

    Transform operator*(const Transform& other) const;

    bool SwapsHandedness() const;

    template <typename T>
    math::Vector3<T> operator()(const math::Vector3<T>& v) const;
    template <typename T>
    math::Point3<T> operator()(const math::Point3<T>& p) const;
    template <typename T>
    math::Point4<T> operator()(const math::Point4<T>& p) const;
    template <typename T>
    inline math::Normal3<T> operator()(const math::Normal3<T>& n) const;
    Bounds3r operator()(const Bounds3r& b) const;

    template <typename T>
    inline math::Point3<T> operator()(const math::Point3<T>& pt, math::Vector3<T>* absError) const;
    template <typename T>
    inline math::Point3<T> operator()(const math::Point3<T>& p, const math::Vector3<T>& pError, math::Vector3<T>* absError) const;
    template <typename T>
    inline math::Vector3<T> operator()(const math::Vector3<T>& v, math::Vector3<T>* absError) const;
    template <typename T>
    inline math::Vector3<T> operator()(const math::Vector3<T>& v, const math::Vector3<T>& vError, math::Vector3<T>* absError) const;

private:
    math::Matrix4x4 m_Matrix;
    math::Matrix4x4 m_MatrixInverse;
};

Transform Translate(const Vector3r& delta);
Transform Scale(real x, real y, real z);
Transform RotateX(real theta);
Transform RotateY(real theta);
Transform RotateZ(real theta);
Transform Rotate(real theta, const Vector3r& axis);
Transform Rotate(math::Rotator Rotator);
Transform LookAt(const Point3r& pos, const Point3r& look, const Vector3r& up);

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
math::Vector3<T> Transform::operator()(const math::Vector3<T>& v) const
{
    T x = v.X, y = v.Y, z = v.Z;
    return math::Vector3<T>(m_Matrix.Data[0][0] * x + m_Matrix.Data[0][1] * y + m_Matrix.Data[0][2] * z,
                            m_Matrix.Data[1][0] * x + m_Matrix.Data[1][1] * y + m_Matrix.Data[1][2] * z,
                            m_Matrix.Data[2][0] * x + m_Matrix.Data[2][1] * y + m_Matrix.Data[2][2] * z);
}

template <typename T>
math::Point3<T> Transform::operator()(const math::Point3<T>& p) const
{
    T x = p.X, y = p.Y, z = p.Z;

    T xp = m_Matrix.Data[0][0] * x + m_Matrix.Data[0][1] * y + m_Matrix.Data[0][2] * z + m_Matrix.Data[0][3];
    T yp = m_Matrix.Data[1][0] * x + m_Matrix.Data[1][1] * y + m_Matrix.Data[1][2] * z + m_Matrix.Data[1][3];
    T zp = m_Matrix.Data[2][0] * x + m_Matrix.Data[2][1] * y + m_Matrix.Data[2][2] * z + m_Matrix.Data[2][3];
    T wp = m_Matrix.Data[3][0] * x + m_Matrix.Data[3][1] * y + m_Matrix.Data[3][2] * z + m_Matrix.Data[3][3];

    assert(wp != 0);

    if (wp == 1)
    {
        return math::Point3<T>(xp, yp, zp);
    }
    else
    {
        return math::Point3<T>(xp, yp, zp) / wp;
    }
}

template <typename T>
math::Point4<T> Transform::operator()(const math::Point4<T>& p) const
{
    T x = p.X, y = p.Y, z = p.Z;

    T xp = m_Matrix.Data[0][0] * x + m_Matrix.Data[0][1] * y + m_Matrix.Data[0][2] * z + m_Matrix.Data[0][3];
    T yp = m_Matrix.Data[1][0] * x + m_Matrix.Data[1][1] * y + m_Matrix.Data[1][2] * z + m_Matrix.Data[1][3];
    T zp = m_Matrix.Data[2][0] * x + m_Matrix.Data[2][1] * y + m_Matrix.Data[2][2] * z + m_Matrix.Data[2][3];
    T wp = m_Matrix.Data[3][0] * x + m_Matrix.Data[3][1] * y + m_Matrix.Data[3][2] * z + m_Matrix.Data[3][3];

    assert(wp != 0);
    return math::Point4<T>(xp, yp, zp, wp);
}

template <typename T>
inline math::Normal3<T> Transform::operator()(const math::Normal3<T>& n) const
{
    T x = n.X, y = n.Y, z = n.Z;
    return math::Normal3<T>(m_MatrixInverse.Data[0][0] * x + m_MatrixInverse.Data[1][0] * y + m_MatrixInverse.Data[2][0] * z,
                      m_MatrixInverse.Data[0][1] * x + m_MatrixInverse.Data[1][1] * y + m_MatrixInverse.Data[2][1] * z,
                      m_MatrixInverse.Data[0][2] * x + m_MatrixInverse.Data[1][2] * y + m_MatrixInverse.Data[2][2] * z);
}

}  // namespace rndr