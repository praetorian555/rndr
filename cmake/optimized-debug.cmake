# Add OptimizedDebug configuration
SET(CMAKE_CXX_FLAGS_OPTIMIZEDDEBUG
    "/Zi /O2 /Ob2")
SET(CMAKE_C_FLAGS_OPTIMIZEDDEBUG
    "${CMAKE_C_FLAGS_DEBUG}")
SET(CMAKE_EXE_LINKER_FLAGS_OPTIMIZEDDEBUG
    "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
SET(CMAKE_SHARED_LINKER_FLAGS_OPTIMIZEDDEBUG
    "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_OPTIMIZEDDEBUG
    CMAKE_C_FLAGS_OPTIMIZEDDEBUG
    CMAKE_EXE_LINKER_FLAGS_OPTIMIZEDDEBUG
    CMAKE_SHARED_LINKER_FLAGS_OPTIMIZEDDEBUG)