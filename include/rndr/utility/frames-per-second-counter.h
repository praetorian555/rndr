#pragma once

#include "rndr/core/base.h"

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
    explicit FramesPerSecondCounter(float update_interval = 1.0f);

    /**
     * Updates the frames per second counter. Should be called each frame.
     * @param delta_time The time since the last update.
     */
    void Update(float delta_time);

    [[nodiscard]] float GetFramesPerSecond() const { return m_frames_per_second; }

private:
    float m_update_interval;
    float m_time_since_last_update;
    float m_frames_per_second;
    uint32_t m_frames_since_last_update;
};

}  // namespace Rndr
