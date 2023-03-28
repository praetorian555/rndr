#include "rndr/utility/input-layout-builder.h"

#include "rndr/core/log.h"

rndr::InputLayoutBuilder::InputLayoutBuilder()
{
    m_Props.Size = 0;
    m_Props.Data = new InputLayoutProperties[GraphicsConstants::kMaxInputLayoutEntries];
}

rndr::InputLayoutBuilder::~InputLayoutBuilder()
{
    delete[] m_Props.Data;
}

rndr::InputLayoutBuilder& rndr::InputLayoutBuilder::AddBuffer(int BufferIndex,
                                                              DataRepetition Repetition,
                                                              int PerInstanceRate)
{
    auto It = m_Buffers.find(BufferIndex);
    if (It == m_Buffers.end())
    {
        m_Buffers.insert(std::make_pair(BufferIndex, BufferInfo{Repetition, PerInstanceRate}));
    }
    else
    {
        RNDR_LOG_ERROR(
            "InputLayoutBuilder::AddBuffer: Failed since the buffer index is already in use!");
    }

    return *this;
}

rndr::InputLayoutBuilder& rndr::InputLayoutBuilder::AppendElement(int BufferIndex,
                                                                  const std::string& SemanticName,
                                                                  PixelFormat Format)
{
    auto BufferIt = m_Buffers.find(BufferIndex);
    if (BufferIt == m_Buffers.end())
    {
        RNDR_LOG_ERROR(
            "InputLayoutBuilder::AppendElement: Failed since the buffer index is not present, call "
            "AddBuffer!");
        return *this;
    }
    if (m_Props.Size == GraphicsConstants::kMaxInputLayoutEntries)
    {
        RNDR_LOG_ERROR(
            "InputLayoutBuilder::AppendElement: Failed since there are no more slots available!");
        return *this;
    }

    int SemanticIndex = 0;
    auto NameIt = m_Names.find(SemanticName);
    if (NameIt == m_Names.end())
    {
        m_Names.insert(std::make_pair(SemanticName, SemanticIndex));
        NameIt = m_Names.find(SemanticName);
    }
    else
    {
        NameIt->second++;
        SemanticIndex = NameIt->second;
    }

    BufferInfo& Info = BufferIt->second;
    const int Idx = static_cast<int>(m_Props.Size++);
    m_Props[Idx].InputSlot = BufferIndex;
    m_Props[Idx].Repetition = Info.Repetiton;
    m_Props[Idx].InstanceStepRate =
        Info.Repetiton == DataRepetition::PerVertex ? 0 : Info.PerInstanceRate;
    m_Props[Idx].SemanticName = NameIt->first;
    m_Props[Idx].SemanticIndex = SemanticIndex;
    m_Props[Idx].Format = Format;
    m_Props[Idx].OffsetInVertex = Info.EntriesCount == 0 ? 0 : kAppendAlignedElement;
    Info.EntriesCount++;

    return *this;
}

rndr::Span<rndr::InputLayoutProperties> rndr::InputLayoutBuilder::Build()
{
    if (!m_Props)
    {
        delete[] m_Props.Data;
        return Span<InputLayoutProperties>{};
    }

    Span<InputLayoutProperties> Rtn = m_Props;
    m_Props.Data = nullptr;
    m_Props.Size = 0;

    return Rtn;
}
