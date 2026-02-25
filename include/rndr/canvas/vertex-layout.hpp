#pragma once

#include "opal/container/dynamic-array.h"

#include "rndr/canvas/context.hpp"

namespace Rndr
{
namespace Canvas
{

/** Semantic vertex attribute names. */
enum class Attrib : u8
{
    Position,
    Normal,
    UV,
    Color,
    Tangent,
    EnumCount
};

/**
 * Describes the format of vertex data. Separate from Brush because it is intrinsic to the mesh,
 * not the rendering style. Can be inferred from shader reflection or constructed manually.
 */
class VertexLayout
{
public:
    VertexLayout() = default;
    ~VertexLayout();

    VertexLayout(const VertexLayout&) = delete;
    VertexLayout& operator=(const VertexLayout&) = delete;
    VertexLayout(VertexLayout&& other) noexcept;
    VertexLayout& operator=(VertexLayout&& other) noexcept;

    [[nodiscard]] VertexLayout Clone() const;

    /**
     * Add a vertex attribute to the layout.
     * @param attrib Semantic name of the attribute.
     * @param format Data format of the attribute.
     */
    void Add(Attrib attrib, Format format);

    /** @return Total stride in bytes for one vertex. */
    [[nodiscard]] u32 GetStride() const;

    /** @return Number of attributes in the layout. */
    [[nodiscard]] u32 GetAttributeCount() const;

    [[nodiscard]] bool IsValid() const;

private:
    struct Entry
    {
        Attrib attrib = Attrib::Position;
        Format format = Format::Float3;
    };
    Opal::DynamicArray<Entry> m_entries;
};

}  // namespace Canvas
}  // namespace Rndr
