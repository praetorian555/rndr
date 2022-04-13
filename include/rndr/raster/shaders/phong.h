#pragma once

#include "rndr/core/transform.h"
#include "rndr/core/camera.h"
#include "rndr/core/pipeline.h"

#if defined RNDR_RASTER

#include "rndr/raster/rasterimage.h"
#include "rndr/raster/rasterpipeline.h"

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

    void AddPointLight(const Point3r& LightPosition, const Color& LightColor);
    void AddDirectionalLight(const Vector3r& Direction, const Color& LightColor);
    void ClearLights();

    void SetDiffuseImage(Image* DiffuseImage) { m_DiffuseImage = DiffuseImage; }
    void SetSpecularImage(Image* SpecularImage) { m_SpecularImage = SpecularImage; }

    void SetDiffuseColor(const Color& DiffuseColor) { m_Kd = DiffuseColor.ToLinearSpace(); }
    void SetSpecularColor(const Color& SpecularColor) { m_Ks = SpecularColor.ToLinearSpace(); }

    void SetShininess(int Shininess) { m_Shininess = Shininess; }

    void SetCamera(Camera* Camera) { m_Camera = Camera; }

private:
    struct PointLight
    {
        Point3r Position;
        Color Irradiance;
    };

    struct DirectionalLight
    {
        Vector3r Direction;
        Color Irradiance;
    };

private:
    Image* m_DiffuseImage = nullptr;
    Image* m_SpecularImage = nullptr;

    int m_Shininess = 32;
    Color m_Kd = Color::Black;
    Color m_Ks = Color::Black;

    rndr::Point3r m_ViewPosition;

    std::vector<PointLight> m_PointLights;
    std::vector<DirectionalLight> m_DirectionalLights;

    Camera* m_Camera;
};

}  // namespace rndr

#endif // RNDR_RASTER
