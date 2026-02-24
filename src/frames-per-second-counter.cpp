#include "rndr/frames-per-second-counter.hpp"

Rndr::FramesPerSecondCounter::FramesPerSecondCounter(f32 update_interval)
    : m_update_interval(update_interval)
{
}

void Rndr::FramesPerSecondCounter::Update(f32 delta_time)
{
    m_time_since_last_update += delta_time;
    m_frames_since_last_update++;

    if (m_time_since_last_update >= m_update_interval)
    {
        m_frames_per_second = static_cast<f32>(m_frames_since_last_update) / m_time_since_last_update;
        m_frames_since_last_update = 0;
        m_time_since_last_update = 0.0f;
    }
}