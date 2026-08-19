#pragma once

/**
 * Forward declarations of every Forge type. Included by the Forge headers so that they can refer to each
 * other without pulling in the full definition, and without relying on inline elaborated type specifiers
 * such as `Opal::Ref<class Device>`, whose implicit declaration lands in the wrong namespace on MSVC.
 */

namespace Rndr::Forge
{

class GraphicsContext;
class PhysicalDevice;
class Device;
class DeviceQueue;
class Surface;
class SwapChain;
class Texture;
class Sampler;
class Buffer;
class Shader;
class Pipeline;
class DescriptorPool;
class DescriptorSetLayout;
class DescriptorSet;
class CommandBuffer;
class Fence;
class Semaphore;
class FrameContext;

}  // namespace Rndr::Forge
