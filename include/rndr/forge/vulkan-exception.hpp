#pragma once

#include "volk/volk.h"

#include "opal/exceptions.h"

#include "rndr/forge/vulkan-result.hpp"

#include "rndr/types.hpp"

namespace Rndr::Forge
{

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

}  // namespace Rndr::Forge
