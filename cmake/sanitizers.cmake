function(rndr_setup_sanitizers project_options)
    if (MSVC)
        target_compile_options(${project_options} INTERFACE /fsanitize=address /Zi /INCREMENTAL:NO)
        target_compile_definitions(${project_options} INTERFACE _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
        # /INFERASANLIBS tells the linker to pull the ASan runtime import libs
        # (clang_rt.asan_dynamic-*.lib) from the default-lib directives in the instrumented
        # objects. Without it the link fails with unresolved __asan_* externals.
        target_link_options(${project_options} INTERFACE /INFERASANLIBS /INCREMENTAL:NO)
    else ()
        # GCC/Clang link the ASan runtime themselves when the flag is passed at link time,
        # so no equivalent of the MSVC DLL copy step is needed.
        target_compile_options(${project_options} INTERFACE -fsanitize=address -g)
        target_link_options(${project_options} INTERFACE -fsanitize=address)
    endif ()
endfunction()

# Copies the dynamic AddressSanitizer runtime (clang_rt.asan_dynamic-x86_64.dll) next to the
# given executable target so it can be launched outside an x64 Native Tools prompt. The DLL
# ships alongside cl.exe in the MSVC toolset, so CMAKE_CXX_COMPILER points us at the right dir.
function(rndr_copy_asan_runtime target)
    if (MSVC AND RNDR_HARDENING)
        get_filename_component(_msvc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_msvc_bin_dir}/clang_rt.asan_dynamic-x86_64.dll"
                "$<TARGET_FILE_DIR:${target}>"
                VERBATIM)
    endif ()
endfunction()