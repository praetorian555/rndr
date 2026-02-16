#pragma once

#include "volk/volk.h"

#include "opal/exceptions.h"

#include "rndr/types.hpp"

namespace Rndr
{

inline const char* VkResultToString(VkResult result)
{
    switch (result)
    {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY: A fence or query has not yet completed.";
        case VK_TIMEOUT:
            return "VK_TIMEOUT: A wait operation has not completed in the specified time.";
        case VK_EVENT_SET:
            return "VK_EVENT_SET: An event is signaled.";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET: An event is unsignaled.";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE: A return array was too small for the result.";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY: A host memory allocation has failed.";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY: A device memory allocation has failed.";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED: Initialization of an object could not be completed.";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST: The logical or physical device has been lost.";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED: Mapping of a memory object has failed.";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT: A requested layer is not present or could not be loaded.";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT: A requested extension is not supported.";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT: A requested feature is not supported.";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER: The requested version of Vulkan is not supported by the driver.";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS: Too many objects of the type have already been created.";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED: A requested format is not supported on this device.";
        case VK_ERROR_FRAGMENTED_POOL:
            return "VK_ERROR_FRAGMENTED_POOL: A pool allocation has failed due to fragmentation of the pool's memory.";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "VK_ERROR_OUT_OF_POOL_MEMORY: A pool memory allocation has failed.";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "VK_ERROR_INVALID_EXTERNAL_HANDLE: An external handle is not a valid handle of the specified type.";
        case VK_ERROR_FRAGMENTATION:
            return "VK_ERROR_FRAGMENTATION: A descriptor pool creation has failed due to fragmentation.";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: A buffer creation or memory allocation failed because the "
                   "requested address is not available.";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR: A surface is no longer available.";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: The requested window is already in use by Vulkan or another API.";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR: A swap chain no longer matches the surface properties exactly, but can still be "
                   "used to present.";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR: A surface has changed such that it is no longer compatible with the swap "
                   "chain. Recreate the swap chain.";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "VK_ERROR_VALIDATION_FAILED_EXT: A validation layer found an error.";
        default:
            return "Unknown VkResult.";
    }
}

struct VulkanException : Opal::Exception
{
    explicit VulkanException(VkResult result, const char* context)
        : Opal::Exception(Opal::StringEx("Vulkan error in ") + context + ": " + VkResultToString(result)), m_result(result)
    {
    }

    [[nodiscard]] VkResult GetVkResult() const { return m_result; }

private:
    VkResult m_result;
};

}  // namespace Rndr
