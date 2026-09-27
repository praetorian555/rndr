/**
 * 02 - Histogram and prefix sum (Headless compute, headless)
 *
 * Bin a buffer of random values into 256 buckets with atomics, then run an exclusive prefix sum over the bins
 * in a second dispatch. Time both passes.
 *
 * Exercises: CmdFillBuffer, CmdBufferBarrier, BufferBarrier::WriteThenRead, groupshared, InterlockedAdd,
 *            TimestampQueryPool, CmdWriteTimestamp, CmdResetQueryPool.
 *
 * Done when: Bins and scan match the CPU; both pass timings print in microseconds.
 * Stretch:   Track the maximum value seen with a 64-bit InterlockedMax
 *            (DeviceFeatures::shader_buffer_int64_atomics), skipping cleanly where the device lacks it.
 * Watch out: Zero the bins with CmdFillBuffer and then barrier before the dispatch: a fill is a transfer write,
 *            not a compute one.
 */

#include "practice.hpp"

int main()
{
    // Context and device, then the work, then a readback checked against the CPU.
    return 0;
}
