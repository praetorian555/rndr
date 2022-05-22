add_executable(modelviewer modelviewer.cpp)

target_compile_features(modelviewer PRIVATE cxx_std_17)

target_link_libraries(modelviewer PRIVATE rndr)

# Compiler options
target_compile_options(cubes PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
if (RNDR_SANITIZER)
  target_compile_options(rndr PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
endif()

# Enable unity build
if (RNDR_UNITY)
set_target_properties(modelviewer PROPERTIES UNITY_BUILD TRUE)
endif()

set_target_properties(modelviewer PROPERTIES FOLDER Examples)
