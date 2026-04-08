#pragma once

#include "opal/container/ref.h"

#include "rndr/canvas/brush.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/mesh.hpp"
#include "rndr/canvas/shader.hpp"
#include "rndr/canvas/texture.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr::Canvas
{

class Context;

/**
 * Renders a cubemap as a skybox by drawing a full-screen triangle and sampling the cubemap
 * using world-space directions derived from the inverse view-projection matrix.
 */
class CubemapRenderer
{
public:
    explicit CubemapRenderer(Opal::Ref<Context> context);
    ~CubemapRenderer();

    CubemapRenderer(const CubemapRenderer&) = delete;
    CubemapRenderer& operator=(const CubemapRenderer&) = delete;
    CubemapRenderer(CubemapRenderer&&) noexcept = default;
    CubemapRenderer& operator=(CubemapRenderer&&) noexcept = default;

    void Destroy();

    /**
     * Set the cubemap texture to render.
     * @param cubemap Cubemap texture. Must outlive this renderer or until replaced.
     */
    void SetCubemap(const Texture& cubemap);

    /**
     * Load an equirectangular image and convert it to a cubemap. The resulting cubemap is owned
     * by this renderer.
     * @param file_path Path to the equirectangular image file (PNG, JPEG, HDR).
     * @param face_size Size of each cubemap face in pixels. If 0, defaults to half the image height.
     * @param desc Texture descriptor for sampling parameters.
     */
    void SetEquirectangular(const Opal::StringUtf8& file_path, i32 face_size = 0, TextureDesc desc = {});

    /**
     * Record draw commands into the draw list.
     * @param draw_list Draw list to record into.
     * @param inverse_vp Inverse of the view-projection matrix (with translation removed for skybox).
     */
    void Render(DrawList& draw_list, const Matrix4x4f& inverse_vp);

private:
    Opal::Ref<Context> m_context;
    Shader m_shader;
    Brush m_brush;
    Mesh m_mesh;
    Texture m_owned_cubemap;
};

}  // namespace Rndr::Canvas
