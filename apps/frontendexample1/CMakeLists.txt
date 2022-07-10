add_executable(fendexam1 fendexam1.cpp)

target_compile_features(fendexam1 PRIVATE cxx_std_17)

target_link_libraries(fendexam1 PRIVATE rndr)

# Compiler options
target_compile_options(fendexam1 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
if (RNDR_SANITIZER)
  target_compile_options(fendexam1 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
endif()

# Enable unity build
if (RNDR_UNITY)
set_target_properties(fendexam1 PROPERTIES UNITY_BUILD TRUE)
endif()

set_target_properties(fendexam1 PROPERTIES FOLDER Examples)