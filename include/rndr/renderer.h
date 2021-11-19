#pragma once

namespace rndr
{

/**
 * Renderer Interface that all renderer implementations will inherit.
 */
struct Renderer
{
    virtual ~Renderer() = default;
};

}  // namespace rndr