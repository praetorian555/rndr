#include "rndr/trace.hpp"

#include <cstring>

#include "opal/hash.h"
#include "rndr/types.hpp"

#if RNDR_CANVAS
#include "glad/glad.h"
#endif

#if RNDR_FORGE
#include "rndr/forge/command-buffer.hpp"
#endif

Rndr::Trace::ScopedCpuEvent::ScopedCpuEvent(const char* name) : m_name(name)
{
    BeginCpuEvent(m_name);
}

Rndr::Trace::ScopedCpuEvent::~ScopedCpuEvent()
{
    EndCpuEvent(m_name);
}

void Rndr::Trace::BeginCpuEvent(const char*) {}

void Rndr::Trace::EndCpuEvent(const char*) {}

Rndr::Trace::ScopedGpuEvent::ScopedGpuEvent(const char* name) : m_name(name)
{
    BeginGpuEvent(m_name);
}

Rndr::Trace::ScopedGpuEvent::ScopedGpuEvent(Forge::CommandBuffer& command_buffer, const char* name)
    : m_name(name), m_command_buffer(&command_buffer)
{
    BeginGpuEvent(command_buffer, m_name);
}

Rndr::Trace::ScopedGpuEvent::~ScopedGpuEvent()
{
    // Which constructor ran is which backend this closes against, and a null command buffer is what the
    // Canvas one leaves behind.
    if (m_command_buffer != nullptr)
    {
        EndGpuEvent(*m_command_buffer, m_name);
    }
    else
    {
        EndGpuEvent(m_name);
    }
}

void Rndr::Trace::BeginGpuEvent(const char* name)
{
#if RNDR_CANVAS
    const u64 group_id = Opal::Hash::CalcRawArray(reinterpret_cast<const u8*>(name), strlen(name));
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, static_cast<GLuint>(group_id), -1, name);
#else
    RNDR_UNUSED(name);
#endif
}

void Rndr::Trace::EndGpuEvent(const char*)
{
#if RNDR_CANVAS
    glPopDebugGroup();
#endif
}

// A reference to an incomplete type is a valid parameter as long as nothing dereferences it, which is what
// lets these two be defined rather than merely declared in a build without Forge. A caller there has no
// command buffer to pass, so the bodies exist to keep the header honest rather than to be called.
#if RNDR_FORGE
void Rndr::Trace::BeginGpuEvent(Forge::CommandBuffer& command_buffer, const char* name)
{
    // Best effort: an annotation that could not be recorded is not something a frame should react to.
    (void)command_buffer.CmdBeginDebugLabel(name);
}

void Rndr::Trace::EndGpuEvent(Forge::CommandBuffer& command_buffer, const char* name)
{
    // The name is the one the matching begin was given. Vulkan closes the innermost region and reads nothing,
    // so it is here for the reader.
    RNDR_UNUSED(name);
    (void)command_buffer.CmdEndDebugLabel();
}
#else
void Rndr::Trace::BeginGpuEvent(Forge::CommandBuffer&, const char*) {}

void Rndr::Trace::EndGpuEvent(Forge::CommandBuffer&, const char*) {}
#endif
