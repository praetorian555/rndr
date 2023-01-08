#include <catch2/catch_test_macros.hpp>

#include "math/math.h"

#include "rndr/core/memory.h"
#include "rndr/core/rndrcontext.h"

TEST_CASE("SingleObject", "Memory")
{
    rndr::RndrContext* App = new rndr::RndrContext();
    REQUIRE(App != nullptr);

    int* MyInt = RNDR_NEW(int, "My Int", 5);
    REQUIRE(*MyInt == 5);

    struct AggregateType
    {
        int a;
        double b;
    };

    RNDR_DELETE(int, MyInt);

    AggregateType* A = RNDR_NEW(AggregateType, "", 10, 15.2);
    REQUIRE(A->a == 10);
    REQUIRE(A->b == 15.2);

    RNDR_DELETE(AggregateType, A);

    struct AggregateType2
    {
        int a = 0;
        double b = 0;

        AggregateType2(int aa)
        {
            RNDR_UNUSED(aa);
            a = 10;
        }
    };
    AggregateType2* B = RNDR_NEW(AggregateType2, "", 20);
    REQUIRE(B->a == 10);

    RNDR_DELETE(AggregateType2, B);

    delete App;
}

TEST_CASE("ScopePtr", "Memory")
{
    std::unique_ptr<rndr::RndrContext> Ctx = std::make_unique<rndr::RndrContext>();

    {
        rndr::ScopePtr<int> Data = rndr::CreateScoped<int>("", 5);
        REQUIRE(*Data == 5);
    }

    {
        rndr::ScopePtr<int> Data1 = rndr::CreateScoped<int>("", 5);
        REQUIRE(*Data1 == 5);

        rndr::ScopePtr<int> Data2 = std::move(Data1);
        REQUIRE(*Data2 == 5);
        REQUIRE(Data1.Get() == nullptr);

        rndr::ScopePtr<int> Data3 = rndr::CreateScoped<int>("", 10);
        REQUIRE(*Data3 == 10);

        Data3 = std::move(Data2);
        REQUIRE(*Data3 == 5);
        REQUIRE(Data2.Get() == nullptr);
    }

    {
        struct MyData
        {
            int a;
        };

        rndr::ScopePtr<MyData> Data = rndr::CreateScoped<MyData>("", 5);
        REQUIRE(Data->a == 5);
    }
}
