/**
 * s3 - Half and int8 kernel (Side quests, headless)
 *
 * A compute kernel doing its arithmetic in half and packing results to 8-bit, checked against the CPU within
 * half precision.
 *
 * Exercises: DeviceFeatures::shader_float16, shader_int8, SpecializationValue (u8).
 *
 * Done when: Results within half-precision tolerance; skips where the features are missing.
 */

#include "practice.hpp"

int main()
{
    // Context and device, then the work, then a readback checked against the CPU.
    return 0;
}
