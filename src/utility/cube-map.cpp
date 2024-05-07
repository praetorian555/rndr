#include "rndr/utility/cube-map.h"

#include "rndr/core/math.h"

#include "stb_image/stb_image_resize2.h"

// Represents faces in a resulting image.
enum class CubeMapFace
{
    PositiveX = 0,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ,
    EnumCount
};

Rndr::Vector3f FaceCoordinatesToXYZ(int x_result, int y_result, CubeMapFace face_id, int face_size)
{
    const float inv_face_size = 1.0f / static_cast<float>(face_size);
    const float x = 2.0f * static_cast<float>(x_result) * inv_face_size;
    const float y = 2.0f * static_cast<float>(y_result) * inv_face_size;
    switch (face_id)
    {
        case CubeMapFace::PositiveX:
            return {1.0f - x, 1.0f, 1 - y};
        case CubeMapFace::NegativeX:
            return {x - 1.0f, -1.0f, 1.0f - y};
        case CubeMapFace::PositiveY:
            return {y - 1.0f, x - 1.0f, 1.0f};
        case CubeMapFace::NegativeY:
            return {1.0f - y, x - 1.0f, -1.0f};
        case CubeMapFace::PositiveZ:
            return {-1.0f, x - 1.0f, y - 1.0f};
        case CubeMapFace::NegativeZ:
            return {1.0f, x - 1.0f, 1.0f - y};
        default:
            RNDR_ASSERT(false);
            break;
    }
    return {};
}

bool Rndr::CubeMap::ConvertEquirectangularMapToVerticalCross(const Bitmap& in_bitmap, Bitmap& out_bitmap)
{
    if (!in_bitmap.IsValid())
    {
        RNDR_LOG_ERROR("Invalid input bitmap!");
        return false;
    }
    if (in_bitmap.GetWidth() != in_bitmap.GetHeight() * 2)
    {
        RNDR_LOG_ERROR("Input bitmap is not an equirectangular map!");
        return false;
    }

    const int face_size = in_bitmap.GetWidth() / 4;

    const int w = face_size * 3;
    const int h = face_size * 4;

    out_bitmap = Bitmap(w, h, 1, in_bitmap.GetPixelFormat());

    // We are trying to get a vertical cross in a coordinate space where we are at origin looking
    // down the negative Z axis, and it's a right-handed coordinate system. The faces are arranged
    // as follows:
    //
    //   +----+----+----+
    //   |    | +Y |    |
    //   | -X | -Z | +X |
    //   |    | -Y |    |
    //   |    | +Z |    |
    //   +----+----+----+
    //
    // To get these planes from the equirectangular map we are using spherical coordinates where the
    // radius is one. The angles are theta and phi. Theta is the angle between the projection of the
    // vector onto the XY plane and the positive X axis. Phi is the angle between the vector and the
    // positive Z axis. The angles are in radians. The coordinate system is then left-handed. We are
    // looking down the positive X axis, the positive Y axis is to the right, and the positive Z
    // axis is up. So in this coordinate system the plane we are looking at is positive X plane
    // where resulting image X coordinate is growing in the direction of positive Y axis, and the
    // resulting image Y coordinate is growing in the direction of negative Z axis.

    constexpr int k_face_count = static_cast<int>(CubeMapFace::EnumCount);
    const Rndr::StackArray<Vector2i, k_face_count> k_face_offsets = {
        Vector2i(2 * face_size, face_size), Vector2i(0, face_size),        Vector2i(face_size, 0), Vector2i(face_size, 2 * face_size),
        Vector2i(face_size, 3 * face_size), Vector2i(face_size, face_size)};

    const int clamp_w = in_bitmap.GetWidth() - 1;
    const int clamp_h = in_bitmap.GetHeight() - 1;

    for (int face = 0; face < k_face_count; face++)
    {
        for (int x_result = 0; x_result < face_size; x_result++)
        {
            for (int y_result = 0; y_result < face_size; y_result++)
            {
                const CubeMapFace face_id = static_cast<CubeMapFace>(face);
                const Rndr::Vector3f point = FaceCoordinatesToXYZ(x_result, y_result, face_id, face_size);
                const float plane_distance = Math::Sqrt(point.x * point.x + point.y * point.y);
                const float theta = std::atan2(point.y, point.x);
                const float phi = std::atan2(point.z, plane_distance);
                //	float point source coordinates
                const float face_size_float = static_cast<float>(face_size);
                const float uf = 2.0f * face_size_float * (theta + Math::k_pi_float) / Math::k_pi_float;
                const float vf = 2.0f * face_size_float * (Math::k_pi_float / 2.0f - phi) / Math::k_pi_float;
                // 4-samples for bi-linear interpolation
                const int32_t u1 = Math::Clamp(static_cast<int32_t>(Math::Floor(uf)), 0, clamp_w);
                const int32_t v1 = Math::Clamp(static_cast<int32_t>(Math::Floor(vf)), 0, clamp_h);
                const int32_t u2 = Math::Clamp(u1 + 1, 0, clamp_w);
                const int32_t v2 = Math::Clamp(v1 + 1, 0, clamp_h);
                // fractional part
                const float s = uf - static_cast<float>(u1);
                const float t = vf - static_cast<float>(v1);
                // fetch 4-samples
                const Rndr::Vector4f a = in_bitmap.GetPixel(u1, v1);
                const Rndr::Vector4f b = in_bitmap.GetPixel(u2, v1);
                const Rndr::Vector4f c = in_bitmap.GetPixel(u1, v2);
                const Rndr::Vector4f d = in_bitmap.GetPixel(u2, v2);
                // bilinear interpolation
                const Rndr::Vector4f color = a * (1 - s) * (1 - t) + b * (s) * (1 - t) + c * (1 - s) * t + d * (s) * (t);
                out_bitmap.SetPixel(x_result + k_face_offsets[face].x, y_result + k_face_offsets[face].y, 0, color);
            }
        };
    }

    return true;
}

bool Rndr::CubeMap::ConvertVerticalCrossToCubeMapFaces(const Rndr::Bitmap& in_bitmap, Bitmap& out_bitmap)
{
    const int face_width = in_bitmap.GetWidth() / 3;
    const int face_height = in_bitmap.GetHeight() / 4;

    constexpr int k_face_count = static_cast<int>(CubeMapFace::EnumCount);
    out_bitmap = Rndr::Bitmap(face_width, face_height, k_face_count, in_bitmap.GetPixelFormat());

    const uint8_t* src = in_bitmap.GetData();
    const size_t pixel_size = in_bitmap.GetPixelSize();
    uint8_t* dst = out_bitmap.GetData();
    for (int face = 0; face < k_face_count; ++face)
    {
        for (int j = 0; j < face_height; ++j)
        {
            for (int i = 0; i < face_width; ++i)
            {
                int x = 0;
                int y = 0;
                const CubeMapFace face_id = static_cast<CubeMapFace>(face);
                switch (face_id)
                {
                    case CubeMapFace::PositiveX:
                        x = i;
                        y = face_height + j;
                        break;
                    case CubeMapFace::NegativeX:
                        x = 2 * face_width + i;
                        y = 1 * face_height + j;
                        break;
                    case CubeMapFace::PositiveY:
                        x = 2 * face_width - (i + 1);
                        y = 1 * face_height - (j + 1);
                        break;
                    case CubeMapFace::NegativeY:
                        x = 2 * face_width - (i + 1);
                        y = 3 * face_height - (j + 1);
                        break;
                    case CubeMapFace::PositiveZ:
                        x = 2 * face_width - (i + 1);
                        y = in_bitmap.GetHeight() - (j + 1);
                        break;
                    case CubeMapFace::NegativeZ:
                        x = face_width + i;
                        y = face_height + j;
                        break;
                    default:
                        break;
                }

                memcpy(dst, src + (y * in_bitmap.GetWidth() + x) * pixel_size, pixel_size);
                dst += pixel_size;
            }
        }
    }

    return true;
}

namespace
{
// From Henry J. Warren's "Hacker's Delight"
float RadicalInverse_VdC(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;  // / 0x100000000
}

// The i-th point is then computed by
// From http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
Rndr::Vector2f Hammersley2d(uint32_t i, uint32_t num_samples)
{
    return {static_cast<float>(i) / static_cast<float>(num_samples), RadicalInverse_VdC(i)};
}
}  // namespace

bool Rndr::CubeMap::ConvolveDiffuse(const Vector3f* in_data, int in_width, int in_height, int out_width, int out_height, Vector3f* out_data,
                                    int nb_monte_carlo_samples)
{
    // only equirectangular maps are supported
    assert(in_width == 2 * in_height);
    if (in_width != 2 * in_height)
    {
        return false;
    }

    Array<Vector3f> tmp(out_width * out_height);

    stbir_resize(reinterpret_cast<const float*>(in_data), in_width, in_height, 0, reinterpret_cast<float*>(tmp.GetData()), out_width,
                 out_height, 0, STBIR_RGB, STBIR_TYPE_FLOAT, STBIR_EDGE_CLAMP, STBIR_FILTER_CUBICBSPLINE);

    const Vector3f* scratch = tmp.GetData();
    in_width = out_width;
    in_height = out_height;

    for (int y = 0; y != out_height; y++)
    {
        const float theta1 = static_cast<float>(y) / static_cast<float>(out_height) * Math::k_pi_float;
        for (int x = 0; x != out_width; x++)
        {
            const float phi1 = static_cast<float>(x) / static_cast<float>(out_width) * 2 * Math::k_pi_float;
            const Vector3f v1 = Vector3f(sin(theta1) * cos(phi1), sin(theta1) * sin(phi1), cos(theta1));
            Vector3f color = Vector3f(0.0f);
            float weight = 0.0f;
            for (int i = 0; i != nb_monte_carlo_samples; i++)
            {
                const Vector2f h = Hammersley2d(i, nb_monte_carlo_samples);
                const int x1 = static_cast<int32_t>(floor(h.x * static_cast<float>(in_width)));
                const int y1 = static_cast<int32_t>(floor(h.y * static_cast<float>(in_height)));
                const float theta2 = static_cast<float>(y1) / static_cast<float>(in_height) * Math::k_pi_float;
                const float phi2 = static_cast<float>(x1) / static_cast<float>(in_width) * 2 * Math::k_pi_float;
                const Vector3f v2 = Vector3f(sin(theta2) * cos(phi2), sin(theta2) * sin(phi2), cos(theta2));
                const float d = std::max(0.0f, Math::Dot(v1, v2));
                if (d > 0.01f)
                {
                    color += scratch[y1 * in_width + x1] * d;
                    weight += d;
                }
            }
            out_data[y * out_width + x] = color / weight;
        }
    }
    return true;
}
