#include "rndr/trace.hpp"

#include <cstring>

#include "opal/hash.h"
#include "rndr/types.hpp"

#if RNDR_CANVAS
#include "glad/glad.h"
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
    BeginGpuEvent(name);
}

Rndr::Trace::ScopedGpuEvent::~ScopedGpuEvent()
{
    EndGpuEvent(m_name);
}

void Rndr::Trace::BeginGpuEvent(const char* name)
{
#if RNDR_CANVAS
    const u64 group_id = Opal::Hash::CalcRawArray(reinterpret_cast<const u8*>(name), strlen(name));
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, static_cast<GLuint>(group_id), -1, name);
#endif
}

void Rndr::Trace::EndGpuEvent(const char*)
{
#if RNDR_CANVAS
    glPopDebugGroup();
#endif
}
