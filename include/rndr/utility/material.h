#include "rndr/core/containers/array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/enum-flags.h"
#include "rndr/core/math.h"
#include "rndr/core/render-api.h"

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
    // Used only if emissive map is missing.
    Rndr::Vector4f emissive_color = Rndr::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
    // Used only if albedo map is missing.
    Rndr::Vector4f albedo_color = Rndr::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    // Roughness of the surface, for anisotropic materials use the x and y while for isotropic materials the same value will be stored in
    // x and y. Used only if metallic_roughness_map is missing.
    Rndr::Vector4f roughness = Rndr::Vector4f(1.0f, 1.0f, 0.0f, 0.0f);

    float transparency_factor = 1.0f;
    float alpha_test = 0.0f;
    // Used only if metallic_roughness_map is missing.
    float metallic_factor = 0.0f;

    MaterialFlags flags = MaterialFlags::NoFlags;

    ImageId ambient_occlusion_map = k_invalid_image_id;
    // Emissive color.
    ImageId emissive_map = k_invalid_image_id;
    // Base color is stored in the RGB channels and opacity in the A channel.
    ImageId albedo_map = k_invalid_image_id;
    // Roughness is stored in the G channel and metallic in the B channel.
    ImageId metallic_roughness_map = k_invalid_image_id;
    ImageId normal_map = k_invalid_image_id;
    // Not used in a cooked material. In cooked material opacity data will be in the alpha channel of the albedo map.
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
 * @param out_textures Array that should store created textures.
 * @param graphics_context Graphics context used to perform GPU related operations.
 * @param in_texture_paths Array of texture paths.
 * @return True if the material was successfully set up, false otherwise.
 */
bool SetupMaterial(MaterialDescription& in_out_material, Array<Image>& out_textures, const GraphicsContext& graphics_context,
                   const Array<String>& in_texture_paths);

}  // namespace Material

}  // namespace Rndr
