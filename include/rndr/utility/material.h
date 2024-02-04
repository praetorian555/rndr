#include "rndr/core/containers/array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/enum-flags.h"
#include "rndr/core/math.h"

struct aiMaterial;

namespace Rndr
{

enum class MaterialFlags : uint32_t
{
    NoFlags = 0,
    Transparent = 1 << 0,

};
RNDR_ENUM_CLASS_FLAGS(MaterialFlags)

/**
 * Represents id of an image on the GPU.
 */
using ImageId = uint64_t;

constexpr ImageId k_invalid_image_id = 0xFFFFFFFFFFFFFFFF;

/**
 * Description of a single material. It contains information about the material's properties and textures. It does not contain the actual
 * texture data but handles to the textures on the GPU.
 *
 * Note that when material description is loaded from the Assimp material the textures are not loaded and we store only the indices of the
 * texture paths in the array of texture paths. The actual loading of the textures is done later when the material is loaded.
 */
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

namespace Material
{

/**
 * Reads material description from the Assimp material.
 * @param out_description Material description to be filled. Note that texture maps will not contain the handles to actual texture data on
 * the GPU but rather index of a texture path in the array of texture paths.
 * @param out_texture_paths Array of texture paths. The indices of the texture paths in this array are stored in the material description.
 * @param out_opacity_maps Array of paths to opacity maps.
 * @param ai_material Assimp material to read from.
 * @return True if the material description was successfully read, false otherwise.
 */
// TODO: This function should be moved to a separate file.
bool ReadDescription(MaterialDescription& out_description, Array<String>& out_texture_paths, Array<String>& out_opacity_maps,
                     const aiMaterial& ai_material);

bool ConvertAndDownscaleTextures(const std::vector<MaterialDescription>& materials, const String& base_path, Array<String>& texture_paths,
                                 const Array<String>& opacity_maps);

bool WriteOptimizedData(const Array<MaterialDescription>& materials, const Array<String>& texture_paths, const String& file_path);

/**
 * Reads optimized material data from the file. The optimized data contains the material descriptions and the texture paths. The textures
 * are not loaded at this point and the material descriptions contain only the indices of the texture paths in the array of texture paths.
 * @param out_materials Material descriptions to be filled.
 * @param out_texture_paths Array of texture paths to be filled. These will be full paths to the textures.
 * @param file_path Path to the file containing the optimized data.
 * @return True if the optimized data was successfully read, false otherwise.
 */
bool ReadOptimizedData(Array<MaterialDescription>& out_materials, Array<String>& out_texture_paths, const String& file_path);

/**
 * Sets up the material description with the given texture paths. This includes loading the textures and setting up the material
 * description with the handles to the textures on the GPU.
 * @param in_out_material Material description to be set up.
 * @param in_texture_paths Array of texture paths.
 * @return True if the material was successfully set up, false otherwise.
 */
bool SetupMaterial(MaterialDescription& in_out_material, const Array<String>& in_texture_paths);

}  // namespace Material

}  // namespace Rndr
