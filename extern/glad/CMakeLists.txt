add_library(glad src/glad.c src/glad_wgl.c include/glad/glad.h include/glad/glad_wgl.h include/KHR/khrplatform.h)
target_include_directories(glad PUBLIC include)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE CXX)
