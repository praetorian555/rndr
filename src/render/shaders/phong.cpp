#include "rndr/render/shaders/phong.h"

#if defined RNDR_RASTER

void rndr::PhongShader::VertexShader(const InVertexInfo& InInfo, OutVertexInfo& OutInfo)
{
    InVertex* InVertexData = (InVertex*)InInfo.UserVertexData;
    InInstance* InInstanceData = (InInstance*)InInfo.UserInstanceData;

    Point3r PositionWorld = InInstanceData->FromModelToWorld(InVertexData->Position);

    OutInfo.PositionNDCNonEucliean = m_Camera->FromWorldToNDC()(rndr::Point4r(PositionWorld));
    OutVertex* OutVertexData = (OutVertex*)OutInfo.UserVertexData;
    OutVertexData->TexCoords = InVertexData->TexCoords;
    OutVertexData->NormalWorld = InInstanceData->FromModelToWorld(InVertexData->Normal);
    OutVertexData->PositionWorld = PositionWorld;
}

void rndr::PhongShader::FragmentShader(const Triangle& Triangle, const InFragmentInfo& InInfo, OutFragmentInfo& OutInfo)
{
    const Point2r TexCoords = RNDR_INTERPOLATE(Triangle, OutVertex, Point2r, TexCoords, InInfo);
    const Vector2r duvdx = RNDR_DX(Triangle, OutVertex, Point2r, TexCoords, Vector2r, InInfo);
    const Vector2r duvdy = RNDR_DY(Triangle, OutVertex, Point2r, TexCoords, Vector2r, InInfo);

    Color Kd = m_Kd;
    if (m_DiffuseImage)
    {
        Kd = m_DiffuseImage->Sample(TexCoords, duvdx, duvdy);
    }

    Color Ks = m_Ks;
    if (m_SpecularImage)
    {
        Ks = m_SpecularImage->Sample(TexCoords, duvdx, duvdy);
    }

    const Point3r FragmentPosition = RNDR_INTERPOLATE(Triangle, OutVertex, Point3r, PositionWorld, InInfo);
    Normal3r Normal = RNDR_INTERPOLATE(Triangle, OutVertex, Normal3r, NormalWorld, InInfo);
    Normal = Normalize(Normal);
    const Vector3r ViewDirection = Normalize(m_ViewPosition - FragmentPosition);

    OutInfo.Color = Color::Black.ToLinearSpace();
    for (int i = 0; i < m_PointLights.size(); i++)
    {
        // TODO(mkostic): Add attenuation support

        Vector3r LightDirection = m_PointLights[i].Position - FragmentPosition;
        LightDirection = Normalize(LightDirection);
        const Vector3r ReflectedDirection = Reflect(LightDirection, (Vector3r)Normal);

        const real AmbientTerm = 0.01;
        const real DiffuseTerm = std::max(rndr::Dot(Normal, LightDirection), (real)0.0);
        const real Dot = rndr::Dot(ViewDirection, ReflectedDirection);
        const real Max = std::max(Dot, (real)0.0);
        const real SpecularTerm = std::pow(std::max(rndr::Dot(ViewDirection, ReflectedDirection), (real)0.0), m_Shininess);

        OutInfo.Color += (Kd * AmbientTerm + Kd * DiffuseTerm + Ks * SpecularTerm) * m_PointLights[i].Irradiance;
    }

    for (int i = 0; i < m_DirectionalLights.size(); i++)
    {
        Vector3r LightDirection = -m_DirectionalLights[i].Direction;
        LightDirection = Normalize(LightDirection);
        const Vector3r ReflectedDirection = Reflect(LightDirection, (Vector3r)Normal);

        const real AmbientTerm = 0.01;
        const real DiffuseTerm = std::max(rndr::Dot(Normal, LightDirection), (real)0.0);
        const real Dot = rndr::Dot(ViewDirection, ReflectedDirection);
        const real Max = std::max(Dot, (real)0.0);
        const real SpecularTerm = std::pow(std::max(rndr::Dot(ViewDirection, ReflectedDirection), (real)0.0), m_Shininess);

        OutInfo.Color += (Kd * AmbientTerm + Kd * DiffuseTerm + Ks * SpecularTerm) * m_DirectionalLights[i].Irradiance;
    }
}

void rndr::PhongShader::AddPointLight(const Point3r& LightPosition, const Color& LightColor)
{
    m_PointLights.push_back(PointLight{LightPosition, LightColor.ToLinearSpace()});
}

void rndr::PhongShader::AddDirectionalLight(const Vector3r& Direction, const Color& LightColor)
{
    m_DirectionalLights.push_back(DirectionalLight{Direction, LightColor.ToLinearSpace()});
}

void rndr::PhongShader::ClearLights()
{
    m_PointLights.clear();
    m_DirectionalLights.clear();
}

#endif // RNDR_RASTER