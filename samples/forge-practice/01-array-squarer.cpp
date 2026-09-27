/**
 * 01 - Array squarer (Headless compute, headless)
 *
 * Fill a storage buffer with 0..N-1, square every element in a compute shader, read it back and compare against
 * the CPU.
 *
 * Exercises: GraphicsContext::Create, Device::Create, Buffer::Create, UploadToBuffer, Shader::FromSource,
 *            ShaderCache, DescriptorSetLayout, DescriptorSet, Pipeline::Create, CmdDispatch, ImmediateSubmit,
 *            ReadBackBuffer.
 *
 * Done when: Every element matches and the debug message count is zero at exit.
 * Stretch:   Pass the buffer by device address in a push constant instead of a descriptor, and set the
 *            workgroup size with a specialization constant.
 * Watch out: A device made without a surface and without enable_presentation has no present queue; ask GetQueue
 *            for Graphics or Compute only.
 */

#include "practice.hpp"

int main()
{
    // Context and device, then the work, then a readback checked against the CPU.
    return 0;
}
