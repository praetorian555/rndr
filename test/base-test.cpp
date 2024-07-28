#include <catch2/catch2.hpp>

#include "rndr/core/base.h"
#include "rndr/core/definitions.h"
#include "rndr/core/delegate.h"

class CustomLogger : public Rndr::Logger
{
    void Log(const Opal::SourceLocation& source_location,
             Rndr::LogLevel log_level,
             const char* message)
    {
        RNDR_UNUSED(source_location);
        RNDR_UNUSED(log_level);
        RNDR_UNUSED(message);
    }
};

TEST_CASE("Init", "[init]")
{
    SECTION("Default create and destroy")
    {
        REQUIRE(Rndr::Init());
        REQUIRE(Rndr::Destroy());
    }
    SECTION("Create with custom logger")
    {
        CustomLogger custom_logger;
        Rndr::Logger* ptr = &custom_logger;
        REQUIRE(Rndr::Init({.user_logger = Opal::Ref(ptr)}));
        REQUIRE(&Rndr::GetLogger() == ptr);
        REQUIRE(Rndr::Destroy());
    }
}

TEST_CASE("Allocate and free", "[memory]")
{
    SECTION("Default allocate and free")
    {
        REQUIRE(Rndr::Init());
        int* ptr = RNDR_NEW(int, 1);
        REQUIRE(ptr != nullptr);
        REQUIRE(*ptr == 1);
        RNDR_DELETE(int, ptr);
        REQUIRE(Rndr::Destroy());
    }
}

void FuncNoReturn(int a)
{
    REQUIRE(a == 1);
}

int FuncReturn(int a)
{
    REQUIRE(a == 1);
    return a + 1;
}

void FuncNoReturnNoArgs()
{
    REQUIRE(true);
}

TEST_CASE("Delegate", "[delegate]")
{
    SECTION("Lambda")
    {
        SECTION("No return")
        {
            Rndr::Delegate<void(int)> delegate;
            delegate.Bind([](int a) { REQUIRE(a == 1); });
            delegate.Execute(1);
        }
        SECTION("No return no args")
        {
            Rndr::Delegate<void()> delegate;
            delegate.Bind([]() { REQUIRE(true); });
            delegate.Execute();
        }
        SECTION("Return")
        {
            Rndr::Delegate<int(int)> delegate;
            delegate.Bind(
                [](int a)
                {
                    REQUIRE(a == 1);
                    return a + 1;
                });
            REQUIRE(delegate.Execute(1) == 2);
        }
    }
    SECTION("Functor")
    {
        SECTION("No return")
        {
            struct Functor
            {
                void operator()(int a) const { REQUIRE(a == 1); }
            };
            Rndr::Delegate<void(int)> delegate;
            delegate.Bind(Functor());
            delegate.Execute(1);
        }
        SECTION("No return no args")
        {
            struct Functor
            {
                void operator()() const { REQUIRE(true); }
            };
            Rndr::Delegate<void()> delegate;
            delegate.Bind(Functor());
            delegate.Execute();
        }
        SECTION("Return")
        {
            struct Functor
            {
                int operator()(int a) const
                {
                    REQUIRE(a == 1);
                    return a + 1;
                }
            };
            Rndr::Delegate<int(int)> delegate;
            delegate.Bind(Functor());
            REQUIRE(delegate.Execute(1) == 2);
        }
    }
    SECTION("Function")
    {
        SECTION("No return")
        {
            Rndr::Delegate<void(int)> delegate;
            delegate.Bind(&FuncNoReturn);
            delegate.Execute(1);
        }
        SECTION("No return no args")
        {
            Rndr::Delegate<void()> delegate;
            delegate.Bind(&FuncNoReturnNoArgs);
            delegate.Execute();
        }
        SECTION("Return")
        {
            Rndr::Delegate<int(int)> delegate;
            delegate.Bind(&FuncReturn);
            REQUIRE(delegate.Execute(1) == 2);
        }
    }
    SECTION("Is bound")
    {
        Rndr::Delegate<void(int)> delegate;
        REQUIRE(!delegate.IsBound());
        delegate.Bind(&FuncNoReturn);
        REQUIRE(delegate.IsBound());
        delegate.Unbind();
        REQUIRE(!delegate.IsBound());
    }
}

TEST_CASE("Multi delegate", "[delegate]")
{
    SECTION("Lambda")
    {
        SECTION("No return")
        {
            Rndr::MultiDelegate<void(int)> delegate;
            delegate.Bind([](int a) { REQUIRE(a == 1); });
            delegate.Bind([](int a) { REQUIRE(a == 1); });
            delegate.Execute(1);
        }
        SECTION("No return no args")
        {
            Rndr::MultiDelegate<void()> delegate;
            delegate.Bind([]() { REQUIRE(true); });
            delegate.Bind([]() { REQUIRE(true); });
            delegate.Execute();
        }
    }
    SECTION("Functor")
    {
        SECTION("No return")
        {
            struct Functor
            {
                void operator()(int a) const { REQUIRE(a == 1); }
            };
            Rndr::MultiDelegate<void(int)> delegate;
            delegate.Bind(Functor());
            delegate.Bind(Functor());
            delegate.Execute(1);
        }
        SECTION("No return no args")
        {
            struct Functor
            {
                void operator()() const { REQUIRE(true); }
            };
            Rndr::MultiDelegate<void()> delegate;
            delegate.Bind(Functor());
            delegate.Bind(Functor());
            delegate.Execute();
        }
    }
    SECTION("Function")
    {
        SECTION("No return")
        {
            Rndr::MultiDelegate<void(int)> delegate;
            delegate.Bind(&FuncNoReturn);
            delegate.Bind(&FuncNoReturn);
            delegate.Execute(1);
        }
        SECTION("No return no args")
        {
            Rndr::MultiDelegate<void()> delegate;
            delegate.Bind(&FuncNoReturnNoArgs);
            delegate.Bind(&FuncNoReturnNoArgs);
            delegate.Execute();
        }
    }
    SECTION("Is bound and unbind")
    {
        Rndr::MultiDelegate<void(int)> delegate;
        REQUIRE(!delegate.IsAnyBound());
        Rndr::DelegateHandle handle = delegate.Bind(&FuncNoReturn);
        REQUIRE(delegate.IsBound(handle));
        REQUIRE(delegate.IsAnyBound());
        delegate.Unbind(handle);
        REQUIRE(!delegate.IsBound(handle));
        REQUIRE(!delegate.IsAnyBound());
    }
}
