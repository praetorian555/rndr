#include "rndr/core/math.h"
#include "rndr/core/enum-flags.h"

namespace Rndr
{

enum class MaterialFlags : uint32_t
{
    NoFlags = 0,
};
RNDR_ENUM_CLASS_FLAGS(MaterialFlags)

/**
 * Represents id of an image on the GPU.
 */
using ImageId = uint64_t;

constexpr ImageId k_invalid_image_id = 0xFFFFFFFFFFFFFFFF;

struct MaterialDescription final
{
    Rndr::Vector4f emissive_color = Rndr::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
    Rndr::Vector4f albedo_color = Rndr::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    // Roughness of the surface, for anisotropic materials use the x and y while for isotropic materials use only x.
    Rndr::Vector4f roughness = Rndr::Vector4f(1.0f, 1.0f, 0.0f, 0.0f);
    float transparency_factor = 1.0f;
    float alpha_test = 0.0f;
    float metallic_factor = 0.0f;
    MaterialFlags flags = MaterialFlags::NoFlags;
    ImageId ambient_occlusion_map = k_invalid_image_id;
    ImageId emissive_map = k_invalid_image_id;
    ImageId albedo_map = k_invalid_image_id;
    ImageId metallic_roughness_map = k_invalid_image_id;
    ImageId normal_map = k_invalid_image_id;
    ImageId opacity_map = k_invalid_image_id;
};

}  // namespace Rndr