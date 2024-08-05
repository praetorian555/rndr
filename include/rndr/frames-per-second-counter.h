#pragma once

#include "rndr/types.h"

namespace Rndr
{

/**
 * A simple class for counting frames per second.
 */
class FramesPerSecondCounter
{
public:
    /**
     * Creates a new FramesPerSecondCounter.
     * @param update_interval The interval at which the frames per second should be updated.
     */
    explicit FramesPerSecondCounter(f32 update_interval = 1.0f);

    /**
     * Updates the frames per second counter. Should be called each frame.
     * @param delta_time The time since the last update.
     */
    void Update(f32 delta_time);

    [[nodiscard]] f32 GetFramesPerSecond() const { return m_frames_per_second; }

private:
    f32 m_update_interval;
    f32 m_time_since_last_update;
    f32 m_frames_per_second;
    u32 m_frames_since_last_update;
};

}  // namespace Rndr
