add_executable(rndr_test renderapi_test.cpp memory_test.cpp)

target_link_libraries(rndr_test PRIVATE Catch2WithMain rndr)
set_target_properties(rndr_test PROPERTIES FOLDER RNDR)

# Enable unity build
if (RNDR_UNITY)
    set_target_properties(rndr_test PROPERTIES UNITY_BUILD TRUE)
endif()

# All users of this library will need at least C++20
target_compile_features(rndr_test PUBLIC cxx_std_20)

# Compiler options
target_compile_options(rndr_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
if (RNDR_SANITIZER)
  target_compile_options(rndr_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
endif()