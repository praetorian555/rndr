#pragma once

#include "rndr/core/camera.h"
#include "rndr/core/pipeline.h"
#include "rndr/core/transform.h"

#if defined RNDR_RASTER

#include "rndr/raster/rasterimage.h"
#include "rndr/raster/rasterpipeline.h"
#include "rndr/raster/rastersampler.h"

namespace rndr
{

class PhongShader
{
public:
    // How vertex data should be organized in the vertex buffer
    struct InVertex
    {
        Point3r Position;
        Point2r TexCoords;
        Normal3r Normal;
    };

    struct OutVertex
    {
        Point2r TexCoords;
        Point3r PositionWorld;
        Normal3r NormalWorld;
    };

    // How instance data should be organized in the instance buffer
    struct InInstance
    {
        Transform FromModelToWorld;
    };

public:
    void VertexShader(const InVertexInfo& InInfo, OutVertexInfo& OutInfo);
    void FragmentShader(const Triangle& Triangle, const InFragmentInfo& InInfo, OutFragmentInfo& OutInfo);

    void SetViewPosition(const Point3r Position) { m_ViewPosition = Position; }

    void AddPointLight(const Point3r& LightPosition, const Vector3r& LightColor);
    void AddDirectionalLight(const Vector3r& Direction, const Vector3r& LightColor);
    void ClearLights();

    void SetDiffuseImage(Image* DiffuseImage) { m_DiffuseImage = DiffuseImage; }
    void SetSpecularImage(Image* SpecularImage) { m_SpecularImage = SpecularImage; }

    void SetDiffuseColor(const Vector3r& DiffuseColor) { m_Kd = DiffuseColor; }
    void SetSpecularColor(const Vector3r& SpecularColor) { m_Ks = SpecularColor; }

    void SetShininess(int Shininess) { m_Shininess = Shininess; }

    void SetCamera(Camera* Camera) { m_Camera = Camera; }

private:
    struct PointLight
    {
        Point3r Position;
        Vector3r Irradiance;
    };

    struct DirectionalLight
    {
        Vector3r Direction;
        Vector3r Irradiance;
    };

private:
    Sampler2D m_DiffuseImage = nullptr;
    Sampler2D m_SpecularImage = nullptr;

    int m_Shininess = 32;
    Vector3r m_Kd = Colors::Black.XYZ();
    Vector3r m_Ks = Colors::Black.XYZ();

    rndr::Point3r m_ViewPosition;

    std::vector<PointLight> m_PointLights;
    std::vector<DirectionalLight> m_DirectionalLights;

    Camera* m_Camera;
};

}  // namespace rndr

#endif  // RNDR_RASTER
