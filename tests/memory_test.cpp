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

TEST_CASE("Arrays", "Memory")
{
    rndr::RndrContext* App = new rndr::RndrContext();
    REQUIRE(App != nullptr);

    int* MyInts = RNDR_NEW_ARRAY(int, 100, "My Int");
    for (int i = 0; i < 100; i++)
    {
        REQUIRE(MyInts[i] == 0);
    }
    RNDR_DELETE_ARRAY(int, MyInts, 100);

    struct AggregateType
    {
        int a = 2;
        double b = 5.5;
    };

    AggregateType* A = RNDR_NEW_ARRAY(AggregateType, 10, "");
    for (int i = 0; i < 10; i++)
    {
        REQUIRE(A->a == 2);
        REQUIRE(A->b == 5.5);
    }
    RNDR_DELETE_ARRAY(AggregateType, A, 10);

    delete App;
}

TEST_CASE("ScopePtr", "Memory")
{
    std::unique_ptr<rndr::RndrContext> Ctx = std::make_unique<rndr::RndrContext>();

    {
        rndr::ScopePtr<int> Data = RNDR_NEW(int, "", 5);
        REQUIRE(*Data == 5);
    }

    {
        rndr::ScopePtr<int> Data1 = RNDR_NEW(int, "", 5);
        REQUIRE(*Data1 == 5);

        rndr::ScopePtr<int> Data2 = std::move(Data1);
        REQUIRE(*Data2 == 5);
        REQUIRE(Data1.Get() == nullptr);

        rndr::ScopePtr<int> Data3 = RNDR_NEW(int, "", 10);
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

        rndr::ScopePtr<MyData> Data = RNDR_NEW(MyData, "", 5);
        REQUIRE(Data->a == 5);
    }
}
