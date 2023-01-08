#pragma once

#include <string>
#include <memory>

#include "rndr/core/base.h"

namespace rndr
{

class CpuTracer
{
public:
    void Init();
    void ShutDown();

    void AddTrace(const std::string& Name, int64_t StartMicroSeconds, int64_t EndMicroSeconds);

private:
#if RNDR_SPDLOG
    std::shared_ptr<spdlog::logger> m_Logger = nullptr;
#endif  // RNDR_SPDLOG
};

class CpuTrace
{
public:
    explicit CpuTrace(CpuTracer* Tracer, const std::string& Name);
    ~CpuTrace();

    CpuTrace(const CpuTrace& Other) = default;
    CpuTrace& operator=(const CpuTrace& Other) = default;

    CpuTrace(CpuTrace&& Other) = default;
    CpuTrace& operator=(CpuTrace&& Other) = default;

private:
    std::string m_Name;
    int64_t m_StartUS = 0;

    CpuTracer* m_Tracer;
};

}  // namespace rndr
