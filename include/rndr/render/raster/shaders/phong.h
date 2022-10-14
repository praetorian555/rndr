#pragma once

#include "rndr/core/projectioncamera.h"

#include "math/transform.h"

#include "rndr/render/pipeline.h"

#if defined RNDR_RASTER

#include "rndr/render/raster/rasterimage.h"
#include "rndr/render/raster/rasterpipeline.h"
#include "rndr/render/raster/rastersampler.h"

namespace rndr
{

class PhongShader
{
public:
    // How vertex data should be organized in the vertex buffer
    struct InVertex
    {
        math::Point3 Position;
        Point2r TexCoords;
        math::Normal3 Normal;
    };

    struct OutVertex
    {
        Point2r TexCoords;
        math::Point3 PositionWorld;
        math::Normal3 NormalWorld;
    };

    // How instance data should be organized in the instance buffer
    struct InInstance
    {
        Transform FromModelToWorld;
    };

public:
    void VertexShader(const InVertexInfo& InInfo, OutVertexInfo& OutInfo);
    void FragmentShader(const Triangle& Triangle, const InFragmentInfo& InInfo, OutFragmentInfo& OutInfo);

    void SetViewPosition(const math::Point3 Position) { m_ViewPosition = Position; }

    void AddPointLight(const Point3& LightPosition, const Vector3r& LightColor);
    void AddDirectionalLight(const Vector3r& Direction, const Vector3r& LightColor);
    void ClearLights();

    void SetDiffuseImage(Image* DiffuseImage) { m_DiffuseImage = DiffuseImage; }
    void SetSpecularImage(Image* SpecularImage) { m_SpecularImage = SpecularImage; }

    void SetDiffuseColor(const Vector3r& DiffuseColor) { m_Kd = DiffuseColor; }
    void SetSpecularColor(const Vector3r& SpecularColor) { m_Ks = SpecularColor; }

    void SetShininess(int Shininess) { m_Shininess = Shininess; }

    void SetCamera(ProjectionCamera* Camera) { m_Camera = Camera; }

private:
    struct PointLight
    {
        math::Point3 Position;
        math::Vector3 Irradiance;
    };

    struct DirectionalLight
    {
        math::Vector3 Direction;
        math::Vector3 Irradiance;
    };

private:
    Sampler2D m_DiffuseImage = nullptr;
    Sampler2D m_SpecularImage = nullptr;

    int m_Shininess = 32;
    math::Vector3 m_Kd = Colors::Black.XYZ();
    math::Vector3 m_Ks = Colors::Black.XYZ();

    rndr::math::Point3 m_ViewPosition;

    std::vector<PointLight> m_PointLights;
    std::vector<DirectionalLight> m_DirectionalLights;

    ProjectionCamera* m_Camera;
};

}  // namespace rndr

#endif  // RNDR_RASTER
