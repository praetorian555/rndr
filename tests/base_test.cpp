#include <catch2/catch_test_macros.hpp>

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"

bool CustomAllocInit(Rndr::OpaquePtr init_data, Rndr::OpaquePtr* allocator_data)
{
    RNDR_UNUSED(init_data);
    RNDR_UNUSED(allocator_data);
    return true;
}

bool CustomAllocDestroy(Rndr::OpaquePtr allocator_data)
{
    RNDR_UNUSED(allocator_data);
    return true;
}

Rndr::OpaquePtr CustomAllocate(Rndr::OpaquePtr allocator_data, uint64_t size, const char* tag)
{
    RNDR_UNUSED(allocator_data);
    RNDR_UNUSED(tag);
    return malloc(size);
}

void CustomFree(Rndr::OpaquePtr allocator_data, Rndr::OpaquePtr ptr)
{
    RNDR_UNUSED(allocator_data);
    free(ptr);
}

bool CustomLogInit(Rndr::OpaquePtr init_data, Rndr::OpaquePtr* logger_data)
{
    RNDR_UNUSED(init_data);
    RNDR_UNUSED(logger_data);
    return true;
}

bool CustomLogDestroy(Rndr::OpaquePtr logger_data)
{
    RNDR_UNUSED(logger_data);
    return true;
}

void CustomLog(Rndr::OpaquePtr logger_data,
               const char* file,
               int line,
               const char* function,
               Rndr::LogLevel log_level,
               const char* message)
{
    RNDR_UNUSED(logger_data);
    RNDR_UNUSED(file);
    RNDR_UNUSED(line);
    RNDR_UNUSED(function);
    RNDR_UNUSED(log_level);
    RNDR_UNUSED(message);
}

TEST_CASE("Init", "[init]")
{
    SECTION("Default create and destroy")
    {
        REQUIRE(Rndr::Init());
        const Rndr::Allocator& allocator = Rndr::GetAllocator();
        REQUIRE(IsValid(allocator));
        REQUIRE(allocator.allocator_data == nullptr);
        REQUIRE(allocator.init == nullptr);
        REQUIRE(allocator.destroy == nullptr);
        REQUIRE(allocator.allocate == &Rndr::DefaultAllocator::Allocate);
        REQUIRE(allocator.free == &Rndr::DefaultAllocator::Free);
        const Rndr::Logger& logger = Rndr::GetLogger();
        REQUIRE(IsValid(logger));
        REQUIRE(logger.logger_data != nullptr);
        REQUIRE(logger.init == &Rndr::DefaultLogger::Init);
        REQUIRE(logger.destroy == &Rndr::DefaultLogger::Destroy);
        REQUIRE(logger.log == &Rndr::DefaultLogger::Log);
        REQUIRE(Rndr::Destroy());
    }
    constexpr uint64_t k_test_allocator_data = 0xdeadbeefdeadbaaf;
    SECTION("Create with custom allocator")
    {
        Rndr::Allocator allocator;
        allocator.allocator_data = reinterpret_cast<Rndr::OpaquePtr>(k_test_allocator_data);
        allocator.init = &CustomAllocInit;
        allocator.destroy = &CustomAllocDestroy;
        allocator.allocate = &CustomAllocate;
        allocator.free = &CustomFree;
        REQUIRE(Rndr::Init({.user_allocator = allocator}));
        const Rndr::Allocator& rndr_allocator = Rndr::GetAllocator();
        REQUIRE(IsValid(rndr_allocator));
        REQUIRE(rndr_allocator.allocator_data
                == reinterpret_cast<Rndr::OpaquePtr>(k_test_allocator_data));
        REQUIRE(rndr_allocator.init == &CustomAllocInit);
        REQUIRE(rndr_allocator.destroy == &CustomAllocDestroy);
        REQUIRE(rndr_allocator.allocate == &CustomAllocate);
        REQUIRE(rndr_allocator.free == &CustomFree);
        const Rndr::Logger& logger = Rndr::GetLogger();
        REQUIRE(IsValid(logger));
        REQUIRE(logger.logger_data != nullptr);
        REQUIRE(logger.init == &Rndr::DefaultLogger::Init);
        REQUIRE(logger.destroy == &Rndr::DefaultLogger::Destroy);
        REQUIRE(logger.log == &Rndr::DefaultLogger::Log);
        REQUIRE(Rndr::Destroy());
    }
    SECTION("Create with custom logger")
    {
        Rndr::Logger logger;
        logger.logger_data = reinterpret_cast<Rndr::OpaquePtr>(k_test_allocator_data);
        logger.init = &CustomLogInit;
        logger.destroy = &CustomLogDestroy;
        logger.log = &CustomLog;
        REQUIRE(Rndr::Init({.user_logger = logger}));
        const Rndr::Allocator& allocator = Rndr::GetAllocator();
        REQUIRE(IsValid(allocator));
        REQUIRE(allocator.allocator_data == nullptr);
        REQUIRE(allocator.init == nullptr);
        REQUIRE(allocator.destroy == nullptr);
        REQUIRE(allocator.allocate == &Rndr::DefaultAllocator::Allocate);
        REQUIRE(allocator.free == &Rndr::DefaultAllocator::Free);
        const Rndr::Logger& rndr_logger = Rndr::GetLogger();
        REQUIRE(IsValid(rndr_logger));
        REQUIRE(rndr_logger.logger_data
                == reinterpret_cast<Rndr::OpaquePtr>(k_test_allocator_data));
        REQUIRE(rndr_logger.init == &CustomLogInit);
        REQUIRE(rndr_logger.destroy == &CustomLogDestroy);
        REQUIRE(rndr_logger.log == &CustomLog);
        REQUIRE(Rndr::Destroy());
    }
}

TEST_CASE("Allocate and free", "[memory]")
{
    SECTION("Default allocate and free")
    {
        REQUIRE(Rndr::Init());
        Rndr::OpaquePtr ptr = Rndr::Allocate(1, "test");
        REQUIRE(ptr != nullptr);
        Rndr::Free(ptr);
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
                void operator()(int a) const
                {
                    REQUIRE(a == 1);
                }
            };
            Rndr::Delegate<void(int)> delegate;
            delegate.Bind(Functor());
            delegate.Execute(1);
        }
        SECTION("No return no args")
        {
            struct Functor
            {
                void operator()() const
                {
                    REQUIRE(true);
                }
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
                void operator()(int a) const
                {
                    REQUIRE(a == 1);
                }
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
                void operator()() const
                {
                    REQUIRE(true);
                }
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
