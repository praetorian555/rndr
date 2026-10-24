#include "builder/rules.hpp"

// The Vulkan headers of the SDK rndr builds against, so the build does not depend on an installed SDK.
BUILDER_EXTERNAL(VulkanHeaders, e, t)
{
    e.system_includes = {"include"};
}
