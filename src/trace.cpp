#include "rndr/trace.h"

#if RNDR_OPENGL
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
#if RNDR_OPENGL
    static GLuint s_group_id = 0;
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, s_group_id++, -1, name);
#endif
}

void Rndr::Trace::EndGpuEvent(const char*)
{
#if RNDR_OPENGL
    glPopDebugGroup();
#endif
}
