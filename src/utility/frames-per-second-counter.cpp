#include "rndr/utility/frames-per-second-counter.h"

Rndr::FramesPerSecondCounter::FramesPerSecondCounter(float update_interval)
    : m_update_interval(update_interval), m_time_since_last_update(0.0f),  m_frames_per_second(0.0f), m_frames_since_last_update(0)
{
}

void Rndr::FramesPerSecondCounter::Update(float delta_time)
{
    m_time_since_last_update += delta_time;
    m_frames_since_last_update++;

    if (m_time_since_last_update >= m_update_interval)
    {
        m_frames_per_second = static_cast<float>(m_frames_since_last_update) / m_time_since_last_update;
        m_frames_since_last_update = 0;
        m_time_since_last_update = 0.0f;
    }
}