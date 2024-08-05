#pragma once

namespace Rndr::Trace
{

struct ScopedEvent
{
    explicit ScopedEvent(const char* name);
    ~ScopedEvent();

private:
    const char* m_name;
};

void BeginEvent(const char* name);
void EndEvent(const char* name);

} // namespace Rndr::Trace


#define __RNDR_CPU_EVENT_SCOPED_1(name, number) const Rndr::Trace::ScopedEvent scoped_event_##number(name)
#define __RNDR_CPU_EVENT_SCOPED(name, number) __RNDR_CPU_EVENT_SCOPED_1(name, number)
#define RNDR_CPU_EVENT_SCOPED(name) __RNDR_CPU_EVENT_SCOPED(name, __LINE__)
#define RNDR_CPU_EVENT_BEGIN(name) Rndr::Trace::BeginEvent(name)
#define RNDR_CPU_EVENT_END(name) Rndr::Trace::EndEvent(name)

// TODO: Implement GPU event tracing
#define RNDR_GPU_EVENT_SCOPED(name)
#define RNDR_GPU_EVENT_BEGIN(name)
#define RNDR_GPU_EVENT_END(name)
