#include "rndr/core/definitions.h"

#include "rndr/core/bitmap.h"

namespace Rndr
{
namespace CubeMap
{

/**
 * Given a equirectangular map, convert it to a vertical cross bitmap.
 * @param in_bitmap Input equirectangular map.
 * @param out_bitmap Output vertical cross bitmap.
 * @return Returns true if conversion was successful, false otherwise.
 * @note Resulting vertical cross bitmap will have the following layout:
 *   +----+----+----+
 *   |    | +Y |    |
 *   | -X | -Z | +X |
 *   |    | -Y |    |
 *   |    | +Z |    |
 *   +----+----+----+
 */
bool ConvertEquirectangularMapToVerticalCross(const Bitmap& in_bitmap, Bitmap& out_bitmap);

/**
 * Given a vertical cross bitmap, convert it to a bitmap containing cube map faces. The expect layout is the same one
 * that is produced by ConvertEquirectangularMapToVerticalCross.
 * @param in_bitmap Input vertical cross bitmap.
 * @param out_bitmap Output bitmap containing cube map faces.
 * @return Returns true if conversion was successful, false otherwise.
 */
bool ConvertVerticalCrossToCubeMapFaces(const Rndr::Bitmap& in_bitmap, Bitmap& out_bitmap);

}  // namespace CubeMap
}  // namespace Rndr