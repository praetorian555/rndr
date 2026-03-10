#include "rndr/canvas/vertex-layout.hpp"

namespace
{

Rndr::u32 FormatByteSize(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::Float1:
            return 4;
        case Rndr::Canvas::Format::Float2:
            return 8;
        case Rndr::Canvas::Format::Float3:
            return 12;
        case Rndr::Canvas::Format::Float4:
            return 16;
        case Rndr::Canvas::Format::Int1:
            return 4;
        case Rndr::Canvas::Format::Int2:
            return 8;
        case Rndr::Canvas::Format::Int3:
            return 12;
        case Rndr::Canvas::Format::Int4:
            return 16;
        default:
            return 0;
    }
}

}  // namespace

Rndr::Canvas::VertexLayout::~VertexLayout() = default;

Rndr::Canvas::VertexLayout::VertexLayout(VertexLayout&& other) noexcept : m_entries(std::move(other.m_entries)) {}

Rndr::Canvas::VertexLayout& Rndr::Canvas::VertexLayout::operator=(VertexLayout&& other) noexcept
{
    if (this != &other)
    {
        m_entries = std::move(other.m_entries);
    }
    return *this;
}

Rndr::Canvas::VertexLayout Rndr::Canvas::VertexLayout::Clone() const
{
    VertexLayout copy;
    for (u64 i = 0; i < m_entries.GetSize(); ++i)
    {
        copy.m_entries.PushBack(m_entries[i]);
    }
    return copy;
}

void Rndr::Canvas::VertexLayout::Add(Attrib attrib, Format format)
{
    Entry entry;
    entry.attrib = attrib;
    entry.format = format;
    m_entries.PushBack(entry);
}

Rndr::u32 Rndr::Canvas::VertexLayout::GetStride() const
{
    u32 stride = 0;
    for (u64 i = 0; i < m_entries.GetSize(); ++i)
    {
        stride += FormatByteSize(m_entries[i].format);
    }
    return stride;
}

Rndr::u32 Rndr::Canvas::VertexLayout::GetAttributeCount() const
{
    return static_cast<u32>(m_entries.GetSize());
}

const Rndr::Canvas::VertexLayout::Entry& Rndr::Canvas::VertexLayout::GetAttribute(u32 index) const
{
    return m_entries[static_cast<u64>(index)];
}

bool Rndr::Canvas::VertexLayout::IsValid() const
{
    return !m_entries.IsEmpty();
}
