#include "rndr/core/threading.h"

static rndr::ThreadingConfig s_Config;
static bool s_bIsInitialized = false;

void rndr::SetupThreading(const ThreadingConfig& Config)
{
    s_Config = Config;
    s_bIsInitialized = true;
}

void rndr::ForEach(int EndIndex, Callback1D Callback, int StartIndex, int BatchSize, int StepSize)
{
    assert(s_bIsInitialized);

    if (s_Config.ThreadCount == 1)
    {
        for (int i = StartIndex; i < EndIndex; i += StepSize)
        {
            Callback(i);
        }
        return;
    }
}

void rndr::ForEach(const Point2i& EndPoint,
                   int Stride,
                   Callback2Di Callback,
                   const Point2i& StartPoint,
                   int BatchSize)
{
    assert(s_bIsInitialized);

    if (s_Config.ThreadCount == 1)
    {
        for (int Y = StartPoint.Y; Y < EndPoint.Y; Y++)
        {
            const int StartX = (Y == StartPoint.Y) ? StartPoint.X : 0;
            const int EndX = (Y == EndPoint.Y - 1) ? EndPoint.X : Stride;
            for (int X = StartX; X < EndX; X++)
            {
                Callback(X, Y);
            }
        }
        return;
    }
}

void rndr::ForEach(const Point2r& EndPoint,
                   int Stride,
                   Callback2Dr Callback,
                   const Point2r& StartPoint,
                   int BatchSize)
{
    assert(s_bIsInitialized);

    if (s_Config.ThreadCount == 1)
    {
        for (real Y = StartPoint.Y; Y < EndPoint.Y; Y++)
        {
            for (real X = StartPoint.X; X < EndPoint.X; X++)
            {
                Callback(X, Y);
            }
        }
        return;
    }
}
