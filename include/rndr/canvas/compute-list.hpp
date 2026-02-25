#pragma once

#include "rndr/types.hpp"

namespace Rndr
{
namespace Canvas
{

class Shader;

/**
 * Records compute dispatches, then executes and resets. Same single-use semantics as DrawList.
 */
class ComputeList
{
public:
    ComputeList() = default;
    ~ComputeList();

    ComputeList(const ComputeList&) = delete;
    ComputeList& operator=(const ComputeList&) = delete;
    ComputeList(ComputeList&& other) noexcept;
    ComputeList& operator=(ComputeList&& other) noexcept;

    /**
     * Record a compute dispatch.
     * @param shader Compute shader to dispatch.
     * @param group_count_x Number of work groups in X.
     * @param group_count_y Number of work groups in Y.
     * @param group_count_z Number of work groups in Z.
     */
    void Dispatch(const Shader& shader, u32 group_count_x, u32 group_count_y, u32 group_count_z);

    /** Execute all recorded dispatches and clear internal state. */
    void Execute();

private:
};

}  // namespace Canvas
}  // namespace Rndr
