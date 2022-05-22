add_executable(cubes cubes.cpp boxpass.cpp boxpass.h)

target_compile_features(cubes PRIVATE cxx_std_17)

target_link_libraries(cubes PRIVATE rndr)

set_target_properties(cubes PROPERTIES FOLDER Examples)

# Enable unity build
if (RNDR_UNITY)
set_target_properties(cubes PROPERTIES UNITY_BUILD TRUE)
endif()

# Compiler options
target_compile_options(cubes PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
if (RNDR_SANITIZER)
  target_compile_options(rndr PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
endif()
