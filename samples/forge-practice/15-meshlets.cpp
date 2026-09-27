/**
 * 15 - Meshlets (GPU-driven, windowed)
 *
 * Split a mesh into meshlets offline, cull them in a task shader and emit them from a mesh shader, fed by a
 * culling pass through indirect count.
 *
 * Exercises: DeviceFeatures::mesh_shader, task_shader, CmdDrawMeshTasks, CmdDrawMeshTasksIndirectCount,
 *            DrawMeshTasksIndirectCommand.
 *
 * Done when: The mesh renders through meshlets only, with a debug colour per meshlet.
 * Stretch:   Cone culling of back-facing meshlets in the task shader.
 * Watch out: Mesh shaders are an extension; the sample must skip with a message on a device without them.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
