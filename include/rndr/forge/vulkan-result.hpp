#pragma once

#include "volk/volk.h"

#include "rndr/error-codes.hpp"
#include "rndr/log.hpp"
#include "rndr/types.hpp"

namespace Rndr::Forge
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

/**
 * The code a VkResult reports as. Everything Vulkan says went wrong lands on one of a handful of codes; the
 * VkResult itself is what the log carries, so nothing here has to distinguish two ways of running out of
 * memory.
 *
 * @return ErrorCode::Success for a result that is not a failure, ErrorCode::OutOfMemory when host or device
 *         memory ran out, ErrorCode::OutOfResources when a pool has nothing left, ErrorCode::DeviceLost when
 *         the device or the surface went away, ErrorCode::FeatureNotSupported when a layer, extension,
 *         feature, format or driver version is not there, or ErrorCode::GraphicsAPIError otherwise.
 */
inline ErrorCode VkResultToErrorCode(VkResult result)
{
    switch (result)
    {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return ErrorCode::OutOfMemory;
        case VK_ERROR_OUT_OF_POOL_MEMORY:
        case VK_ERROR_FRAGMENTED_POOL:
        case VK_ERROR_FRAGMENTATION:
        case VK_ERROR_TOO_MANY_OBJECTS:
            return ErrorCode::OutOfResources;
        case VK_ERROR_DEVICE_LOST:
        case VK_ERROR_SURFACE_LOST_KHR:
            return ErrorCode::DeviceLost;
        case VK_ERROR_LAYER_NOT_PRESENT:
        case VK_ERROR_EXTENSION_NOT_PRESENT:
        case VK_ERROR_FEATURE_NOT_PRESENT:
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return ErrorCode::FeatureNotSupported;
        default:
            return result < 0 ? ErrorCode::GraphicsAPIError : ErrorCode::Success;
    }
}

}  // namespace Rndr::Forge

/**
 * Run a Vulkan call, and on failure log the result together with the name of the function it came from and
 * return the code it maps to. For a function that returns Rndr::ErrorCode.
 *
 * @param expr A call returning VkResult.
 * @param function_name Name of the Vulkan function, for the log line.
 */
#define RNDR_FORGE_VK_CHECK(expr, function_name)                                                                  do                                                                                                            {                                                                                                                 const VkResult vk_result_ = (expr);                                                                            if (vk_result_ != VK_SUCCESS)                                                                                  {                                                                                                                 RNDR_LOG_ERROR("Forge: {} failed: {}", function_name, Rndr::Forge::VkResultToString(vk_result_));               return Rndr::Forge::VkResultToErrorCode(vk_result_);                                                       }                                                                                                         } while (0)

/**
 * The same, for a function that returns an Opal::Expected. The error is wrapped in @p ResultType, which is the
 * Expected the enclosing function returns.
 */
#define RNDR_FORGE_VK_CHECK_EXPECTED(expr, function_name, ResultType)                                             do                                                                                                            {                                                                                                                 const VkResult vk_result_ = (expr);                                                                            if (vk_result_ != VK_SUCCESS)                                                                                  {                                                                                                                 RNDR_LOG_ERROR("Forge: {} failed: {}", function_name, Rndr::Forge::VkResultToString(vk_result_));               return ResultType(Rndr::Forge::VkResultToErrorCode(vk_result_));                                           }                                                                                                         } while (0)
