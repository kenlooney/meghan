file(GLOB_RECURSE MEGLANG_SOURCES CONFIGURE_DEPENDS "src/*.c" "include/*.h")
add_library(meglang SHARED ${MEGLANG_SOURCES})
target_include_directories(meglang PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(meglang PUBLIC c_std_11)

set_target_properties(meglang PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(MSVC)
  target_compile_definitions(meglang PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(meglang PRIVATE /W4)
else()
  target_compile_options(meglang PRIVATE -Wall -Wextra -Wpedantic)
endif()
