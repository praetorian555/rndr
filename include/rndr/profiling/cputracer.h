#pragma once

#include <string>

#include "rndr/core/base.h"

namespace rndr
{

class CpuTracer
{
public:
    static CpuTracer* Get();

    void Init();
    void ShutDown();

    void AddTrace(const std::string& Name, int64_t StartMicroSeconds, int64_t EndMicroSeconds);

private:
    static std::unique_ptr<CpuTracer> s_Tracer;
};

class CpuTrace
{
public:
    explicit CpuTrace(const std::string& Name);
    ~CpuTrace();

private:
    std::string m_Name;
    int64_t m_StartUS = 0;
};

#define RNDR_CPU_TRACE(Name) rndr::CpuTrace CpuTrace_Var{Name};

}  // namespace rndr
