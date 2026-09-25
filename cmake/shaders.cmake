# Compiling Slang to SPIR-V at build time, for a build that cannot compile at run time (Android, which has no
# Slang - see RNDR_SHADER_COMPILER). Runs the host's slangc, found in cmake/dependencies.cmake as RNDR_SLANGC.
#
# The options are the ones ShaderCompiler sets on its session for SPIR-V (LoadModule in
# src/core/shader-compiler.cpp), and the two lists have to stay equal: a shader compiled here is otherwise not
# the one the desktop compiles from the same source. The [forge] case "Forge slangc and ShaderCompiler produce
# the same module" runs slangc with this list and compares the bytes.
set(RNDR_SLANGC_OPTIONS -target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name -matrix-layout-row-major)

# Where rndr_compile_shader writes, one directory per target. Settable, so that whatever packages the files - the
# Gradle build in samples/android - can name a directory it knows rather than one under a build tree it does not.
if (NOT DEFINED RNDR_SPIRV_DIR)
    set(RNDR_SPIRV_DIR ${CMAKE_BINARY_DIR}/spirv)
endif ()

# Compiles one entry point of a Slang source into ${RNDR_SPIRV_DIR}/<target>/<source name>.<entry point>.spv
# and makes the target depend on it.
#   target      - the target that needs the SPIR-V
#   source      - the .slang file, relative to the calling CMakeLists.txt or absolute
#   entry_point - the entry point's name, which it keeps in the module
#   stage       - vertex, fragment or compute
function(rndr_compile_shader target source entry_point stage)
    if (NOT DEFINED RNDR_SLANGC)
        message(FATAL_ERROR "rndr_compile_shader needs RNDR_SLANGC, which is only set when RNDR_FORGE is ON")
    endif ()
    get_filename_component(source_path ${source} ABSOLUTE)
    get_filename_component(source_name ${source} NAME_WE)
    set(output_dir ${RNDR_SPIRV_DIR}/${target})
    set(output ${output_dir}/${source_name}.${entry_point}.spv)
    add_custom_command(
            OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
            COMMAND ${RNDR_SLANGC} ${source_path} ${RNDR_SLANGC_OPTIONS} -entry ${entry_point} -stage ${stage} -o ${output}
            DEPENDS ${source_path}
            COMMENT "Compiling ${source_name}.slang:${entry_point} to SPIR-V"
            VERBATIM)
    target_sources(${target} PRIVATE ${output})
endfunction()
