#pragma once

#include "opal/container/string.h"

#include "rndr/types.hpp"

namespace Rndr
{

/** Whether the application may read the device's precise location. */
enum class LocationPermission : u8
{
    /** Not granted: never asked, refused, or only the approximate location was allowed. */
    Denied,
    /** The precise location may be read while the application is in use. */
    Granted,
};

/** One position the device's GPS reported. */
struct LocationFix
{
    f64 latitude_degrees = 0.0;
    f64 longitude_degrees = 0.0;
    /** The radius, in meters, the position is 68% likely to be within; negative when the fix does not say. */
    f32 accuracy_meters = -1.0f;
    /** The speed over ground in meters per second; negative when the fix does not say. */
    f32 speed_meters_per_second = -1.0f;
    /**
     * When the fix was taken, in seconds on a clock that keeps running while the device sleeps and has an arbitrary
     * start. Only the difference between two fixes means anything.
     */
    f64 time_seconds = 0.0;
};

/** How to track the location. See Application::StartLocationUpdates. */
struct LocationUpdatesDesc
{
    /** How often to ask for a fix, in milliseconds. The GPS may deliver more or less often. */
    u32 interval_ms = 1000;
    /**
     * Keep tracking while the application is in the background or the screen is off. On Android that runs a
     * foreground service, which shows a notification with the title and text below for as long as it tracks, and which
     * the manifest must declare (see docs/android-plan.md).
     */
    bool keep_running_in_background = false;
    Opal::StringUtf8 notification_title;
    Opal::StringUtf8 notification_text;
};

}  // namespace Rndr
