#include "rndr/core/trace.h"

Rndr::Trace::ScopedEvent::ScopedEvent(const char* name) : m_name(name)
{
    BeginEvent(m_name);
}

Rndr::Trace::ScopedEvent::~ScopedEvent()
{
    EndEvent(m_name);
}

void Rndr::Trace::BeginEvent(const char*) {}

void Rndr::Trace::EndEvent(const char*) {}