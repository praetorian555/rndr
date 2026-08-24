function(rndr_setup_compiler_options target)

    # /MP (parallel compilation within cl.exe) and UNICODE (wide-char Win32 API) mean nothing
    # to GCC/Clang, so keep them MSVC-only.
    target_compile_options(${target} INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:/MP;-DUNICODE=1>")

endfunction()
