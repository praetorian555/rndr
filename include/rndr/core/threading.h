#include "rndr/core/rndr.h"

#include <functional>

#include "rndr/core/math.h"

namespace rndr
{

struct ThreadingConfig
{
    int ThreadCount = 1;
};

void SetupThreading(const ThreadingConfig& Config = ThreadingConfig{});

using Callback1D = std::function<void(int Index)>;
using Callback2Di = std::function<void(int X, int Y)>;
using Callback2Dr = std::function<void(real X, real Y)>;

void ForEach(int EndIndex,
             Callback1D Callback,
             int StartIndex = 0,
             int BatchSize = 1,
             int StepSize = 1);
void ForEach(const Point2i& EndPoint,
             int Stride,
             Callback2Di Callback,
             const Point2i& StartPoint = Point2i{0, 0},
             int BatchSize = 1);
void ForEach(const Point2r& EndPoint,
             int Stride,
             Callback2Dr Callback,
             const Point2r& StartPoint = Point2r{0.5, 0.5},
             int BatchSize = 1);

}  // namespace rndr