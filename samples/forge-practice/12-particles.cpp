/**
 * 12 - Particle system (GPU-driven, windowed)
 *
 * Simulate particles in compute, append the survivors to a list with an atomic counter, and draw exactly that
 * many with CmdDrawIndirectCount.
 *
 * Exercises: CmdFillBuffer, CmdDispatch, BufferUsageBits::Indirect, CmdDrawIndirectCount, draw_indirect_count,
 *            BufferBarrier (compute to indirect).
 *
 * Done when: Emitter spawns and particles die without the CPU ever reading the count.
 * Stretch:   Move the simulation to the async compute queue with ownership transfer both ways and a timeline
 *            semaphore between the queues.
 * Watch out: The count buffer is read by the indirect stage; the barrier after compute must name DrawIndirect
 *            as the reader.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
