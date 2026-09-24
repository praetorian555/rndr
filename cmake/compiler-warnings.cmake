function(rndr_setup_compiler_warnings target)

    set(MSVC_WARNINGS
        /W4 # Baseline reasonable warnings
        /w14242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
        /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
        /w14263 # 'function': member function does not override any base class virtual member function
        /w14265 # 'classname': class has virtual functions, but destructor is not virtual instances of this class may not
        # be destructed correctly
        /w14287 # 'operator': unsigned/negative constant mismatch
        /we4289 # nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside
        # the for-loop scope
        /w14296 # 'operator': expression is always 'boolean_value'
        /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
        /w14545 # expression before comma evaluates to a function which is missing an argument list
        /w14546 # function call before comma missing argument list
        /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
        /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
        /w14555 # expression has no effect; expected expression with side- effect
        /w14640 # Enable warning on thread un-safe static member initialization
        /w14826 # Conversion from 'type1' to 'type2' is sign-extended. This may cause unexpected runtime behavior.
        /w14905 # wide string literal cast to 'LPSTR'
        /w14906 # string literal cast to 'LPWSTR'
        /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
        /wd4201 # nonstandard extension used: nameless struct/union
        /permissive- # standards conformance mode for MSVC compiler.
    )

    # Warnings as errors
    list(APPEND MSVC_WARNINGS /WX)

    # The GCC and Clang lists are Opal's (cmake/compiler-warnings.cmake in praetorian555/opal), so that
    # Opal's headers, which compile as part of rndr's sources, meet the same flags they were written
    # against. Three exceptions follow the lists, and unlike Opal, warnings are errors here on all three
    # compilers.
    set(CLANG_WARNINGS
        -Wall
        -Wextra # reasonable and standard
        -Wshadow # variable declaration shadows one from a parent scope
        -Wnon-virtual-dtor # class with virtual functions has a non-virtual destructor
        -Wold-style-cast # C-style casts
        -Wcast-align # casts that may be a performance problem
        -Wunused # anything unused
        -Woverloaded-virtual # overload (not override) of a virtual function
        -Wpedantic # non-standard C++
        -Wconversion # type conversions that may lose data
        -Wsign-conversion # sign conversions
        -Wnull-dereference # a null dereference is detected
        -Wdouble-promotion # implicit float to double promotion
        -Wformat=2 # security issues around functions that format output (ie printf)
        -Wimplicit-fallthrough # switch case falls through without an annotation
        -Wno-gnu-anonymous-struct # anonymous structs are a GNU extension; Opal's vectors use them
        -Wno-nested-anon-types # nested anonymous structs, same
        -Wno-dtor-name # template class destructors named without their template arguments, as Opal's are
    )

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation # indentation implies blocks where blocks do not exist
        -Wduplicated-cond # if / else chain has duplicated conditions
        -Wduplicated-branches # if / else branches have duplicated code
        -Wlogical-op # logical operations used where bitwise were probably wanted
        -Wuseless-cast # a cast to the same type
        -Wsuggest-override # an overriding member function is not marked 'override' or 'final'
    )

    # Where rndr departs from Opal. All three fire on idioms this codebase uses everywhere and MSVC at
    # /W4 /WX accepts: designated initializers that leave members at their defaults on purpose - how
    # the Forge descriptors are meant to be filled - and i32 indices and counts against Opal's u64
    # size_type.
    set(RNDR_EXCEPTIONS
        -Wno-missing-field-initializers
        -Wno-sign-compare
        -Wno-sign-conversion
    )
    list(APPEND CLANG_WARNINGS ${RNDR_EXCEPTIONS} -Werror)
    list(APPEND GCC_WARNINGS ${RNDR_EXCEPTIONS} -Werror)

    # C++ only: the one C file compiled into rndr (SPIRV-Reflect) is third-party, and several of these
    # flags mean nothing to a C compiler.
    target_compile_options(${target} INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:${MSVC_WARNINGS}>")
    target_compile_options(${target} INTERFACE "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:${CLANG_WARNINGS}>")
    target_compile_options(${target} INTERFACE "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:${GCC_WARNINGS}>")

endfunction()