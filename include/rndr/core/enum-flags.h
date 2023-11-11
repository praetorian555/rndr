#pragma once

#define RNDR_ENUM_CLASS_FLAGS(Enum)                                                       \
    inline Enum& operator|=(Enum& lhs, Enum rhs)                                          \
    {                                                                                     \
        return lhs = (Enum)((__underlying_type(Enum))lhs | (__underlying_type(Enum))rhs); \
    }                                                                                     \
    inline Enum& operator&=(Enum& lhs, Enum rhs)                                          \
    {                                                                                     \
        return lhs = (Enum)((__underlying_type(Enum))lhs & (__underlying_type(Enum))rhs); \
    }                                                                                     \
    inline Enum& operator^=(Enum& lhs, Enum rhs)                                          \
    {                                                                                     \
        return lhs = (Enum)((__underlying_type(Enum))lhs ^ (__underlying_type(Enum))rhs); \
    }                                                                                     \
    inline constexpr Enum operator|(Enum lhs, Enum rhs)                                   \
    {                                                                                     \
        return (Enum)((__underlying_type(Enum))lhs | (__underlying_type(Enum))rhs);       \
    }                                                                                     \
    inline constexpr Enum operator&(Enum lhs, Enum rhs)                                   \
    {                                                                                     \
        return (Enum)((__underlying_type(Enum))lhs & (__underlying_type(Enum))rhs);       \
    }                                                                                     \
    inline constexpr Enum operator^(Enum lhs, Enum rhs)                                   \
    {                                                                                     \
        return (Enum)((__underlying_type(Enum))lhs ^ (__underlying_type(Enum))rhs);       \
    }                                                                                     \
    inline constexpr bool operator!(Enum E)                                               \
    {                                                                                     \
        return !(__underlying_type(Enum))E;                                               \
    }                                                                                     \
    inline constexpr Enum operator~(Enum E)                                               \
    {                                                                                     \
        return (Enum) ~(__underlying_type(Enum))E;                                        \
    }