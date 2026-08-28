#pragma once

#include "rndr/forge/forward.hpp"

/**
 * Markers that say what a stretch of work is, for a profiler and for a graphics capture. A CPU event brackets
 * a stretch of host time; a GPU event brackets a stretch of the command stream, which a capture shows as one
 * collapsible entry in place of the loose commands inside it.
 *
 * A GPU event is written against whichever rendering API the code recording it uses, and the two APIs do not
 * name the target the same way. OpenGL pushes onto the context that is current on this thread, so Canvas code
 * names only the event. Vulkan has no current anything - a label is a command like any other and has to be
 * recorded into a particular command buffer - so Forge code names that command buffer first:
 *
 * @code
 *   RNDR_GPU_EVENT_SCOPED("shadow pass");                  // Canvas
 *   RNDR_GPU_EVENT_SCOPED(command_buffer, "shadow pass");  // Forge
 * @endcode
 *
 * The macro is one name for both, and the overloads below are what picks the backend from the arguments, so a
 * build with both APIs enabled has both forms available and neither call site has to say which it means. What
 * cannot be shared is the call site itself: code that records GPU work already knows which API it is talking
 * to, and the command buffer is either in scope or the concept does not exist.
 *
 * Everything here is best effort. A marker that could not be recorded - no debug utils extension, no
 * KHR_debug - changes nothing a caller would act on, so none of it reports.
 *
 * These markers are annotations rather than measurements: they tell a capture how to group work, and they do
 * not time it. What the device actually spent is TimestampQueryPool in rndr/forge/query.hpp.
 *
 * A Forge region can also carry a colour, which a capture tool tints the row with. That is Forge's own idea
 * and OpenGL has nothing matching it, so it is not part of these macros - reach for Forge::ScopedDebugLabel
 * directly when a pass wants one.
 */

namespace Rndr::Trace
{

struct ScopedCpuEvent
{
    explicit ScopedCpuEvent(const char* name);
    ~ScopedCpuEvent();

    // A scope, not a value, for the same reason as the GPU one below.
    ScopedCpuEvent(const ScopedCpuEvent&) = delete;
    ScopedCpuEvent& operator=(const ScopedCpuEvent&) = delete;
    ScopedCpuEvent(ScopedCpuEvent&&) = delete;
    ScopedCpuEvent& operator=(ScopedCpuEvent&&) = delete;

private:
    const char* m_name;
};

void BeginCpuEvent(const char* name);
void EndCpuEvent(const char* name);

/**
 * A GPU event region that closes itself, in a Canvas flavour and a Forge one. The command buffer of the Forge
 * flavour is held by pointer and has to outlive the scope, which it does wherever a scope is what it looks
 * like: the region is opened and closed inside one stretch of recording into that buffer.
 */
struct ScopedGpuEvent
{
    /** Canvas: the group is pushed onto the OpenGL context current on this thread. */
    explicit ScopedGpuEvent(const char* name);

    /** Forge: the label is recorded into this command buffer, between the commands the region covers. */
    ScopedGpuEvent(Forge::CommandBuffer& command_buffer, const char* name);

    ~ScopedGpuEvent();

    // A scope, not a value: copying one would close the region twice and moving it would leave the question of
    // which copy owns the end.
    ScopedGpuEvent(const ScopedGpuEvent&) = delete;
    ScopedGpuEvent& operator=(const ScopedGpuEvent&) = delete;
    ScopedGpuEvent(ScopedGpuEvent&&) = delete;
    ScopedGpuEvent& operator=(ScopedGpuEvent&&) = delete;

private:
    const char* m_name;
    Forge::CommandBuffer* m_command_buffer = nullptr;
};

void BeginGpuEvent(const char* name);
void EndGpuEvent(const char* name);

/** Open a named region in this command buffer, covering the commands recorded until the matching end. */
void BeginGpuEvent(Forge::CommandBuffer& command_buffer, const char* name);

/**
 * Close the region the last BeginGpuEvent opened on this command buffer. Regions nest, and every one has to be
 * closed. The name is what the matching begin was given: Vulkan does not read it, and it is here so that the
 * two halves of a region read alike and the end of a long one can be found by searching for its name.
 */
void EndGpuEvent(Forge::CommandBuffer& command_buffer, const char* name = nullptr);

}  // namespace Rndr::Trace

// The scoped macros name a variable nobody writes, so the name has to be unique within the scope. __COUNTER__
// rather than __LINE__: two regions can share a line, and MSVC's traditional preprocessor gives __LINE__ the
// line of the closing parenthesis for everything inside one macro invocation - so a pass nested inside another
// within a single REQUIRE or assertion collides, and the inner declaration shadows the outer one.
//
// Two levels in each, so that __COUNTER__ is expanded to its value before it is pasted into the name.
#define __RNDR_CPU_EVENT_SCOPED_1(number, name) const Rndr::Trace::ScopedCpuEvent scoped_cpu_event_##number(name)
#define __RNDR_CPU_EVENT_SCOPED(number, name) __RNDR_CPU_EVENT_SCOPED_1(number, name)
#define RNDR_CPU_EVENT_SCOPED(name) __RNDR_CPU_EVENT_SCOPED(__COUNTER__, name)
#define RNDR_CPU_EVENT_BEGIN(name) Rndr::Trace::BeginCpuEvent(name)
#define RNDR_CPU_EVENT_END(name) Rndr::Trace::EndCpuEvent(name)

// The number comes first here so that the arguments the call site wrote stay last and can be variadic, which
// is what lets one macro spell both the Canvas form and the Forge one.
#define __RNDR_GPU_EVENT_SCOPED_1(number, ...) const Rndr::Trace::ScopedGpuEvent scoped_gpu_event_##number(__VA_ARGS__)
#define __RNDR_GPU_EVENT_SCOPED(number, ...) __RNDR_GPU_EVENT_SCOPED_1(number, __VA_ARGS__)
#define RNDR_GPU_EVENT_SCOPED(...) __RNDR_GPU_EVENT_SCOPED(__COUNTER__, __VA_ARGS__)
#define RNDR_GPU_EVENT_BEGIN(...) Rndr::Trace::BeginGpuEvent(__VA_ARGS__)
#define RNDR_GPU_EVENT_END(...) Rndr::Trace::EndGpuEvent(__VA_ARGS__)
