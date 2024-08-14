#pragma once

namespace Rndr::Trace
{

struct ScopedCpuEvent
{
    explicit ScopedCpuEvent(const char* name);
    ~ScopedCpuEvent();

private:
    const char* m_name;
};

void BeginCpuEvent(const char* name);
void EndCpuEvent(const char* name);

struct ScopedGpuEvent
{
    explicit ScopedGpuEvent(const char* name);
    ~ScopedGpuEvent();

private:
    const char* m_name;
};

void BeginGpuEvent(const char* name);
void EndGpuEvent(const char* name);

}  // namespace Rndr::Trace

#define __RNDR_CPU_EVENT_SCOPED_1(name, number) const Rndr::Trace::ScopedCpuEvent scoped_cpu_event_##number(name)
#define __RNDR_CPU_EVENT_SCOPED(name, number) __RNDR_CPU_EVENT_SCOPED_1(name, number)
#define RNDR_CPU_EVENT_SCOPED(name) __RNDR_CPU_EVENT_SCOPED(name, __LINE__)
#define RNDR_CPU_EVENT_BEGIN(name) Rndr::Trace::BeginCpuEvent(name)
#define RNDR_CPU_EVENT_END(name) Rndr::Trace::EndCpuEvent(name)

#define __RNDR_GPU_EVENT_SCOPED_1(name, number) const Rndr::Trace::ScopedGpuEvent scoped_gpu_event_##number(name)
#define __RNDR_GPU_EVENT_SCOPED(name, number) __RNDR_GPU_EVENT_SCOPED_1(name, number)
#define RNDR_GPU_EVENT_SCOPED(name) __RNDR_GPU_EVENT_SCOPED(name, __LINE__)
#define RNDR_GPU_EVENT_BEGIN(name) Rndr::Trace::BeginGpuEvent(name)
#define RNDR_GPU_EVENT_END(name) Rndr::Trace::EndGpuEvent(name)
