#pragma once

#include <map>

#include "rndr/core/base.h"
#include "rndr/render/graphicstypes.h"

namespace rndr
{

struct GraphicsContext;
struct Shader;

struct InputLayoutBuilder
{
    InputLayoutBuilder();
    ~InputLayoutBuilder();

    InputLayoutBuilder(const InputLayoutBuilder& Other) = default;
    InputLayoutBuilder& operator=(const InputLayoutBuilder& Other) = default;

    InputLayoutBuilder(InputLayoutBuilder&& Other) = default;
    InputLayoutBuilder& operator=(InputLayoutBuilder&& Other) = default;

    InputLayoutBuilder& AddBuffer(int BufferIndex, DataRepetition Repetition, int PerInstanceRate);
    // If the same SemanticName is used twice in one builder instance this will increment underlying
    // semantic index
    InputLayoutBuilder& AppendElement(int BufferIndex,
                                      const std::string& SemanticName,
                                      PixelFormat Format);

    Span<InputLayoutProperties> Build();

private:
    struct BufferInfo
    {
        DataRepetition Repetiton;
        int PerInstanceRate;
        int EntriesCount = 0;
    };

    std::map<int, BufferInfo> m_Buffers;
    std::map<std::string, int> m_Names;
    rndr::Span<InputLayoutProperties> m_Props;
};

}  // namespace rndr
