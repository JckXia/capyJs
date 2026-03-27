cmake_minimum_required(VERSION 3.10)
project(capyJs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

#----------------------
# libuv (vendored)
#----------------------
set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)  # Static lib only
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)   # Skip their tests
add_subdirectory(deps/libuv)

#----------------------
# QuickJS configs
#----------------------
set(QJS_DIR ${CMAKE_SOURCE_DIR}/deps/quickjs)
add_library(quickjs STATIC
    ${QJS_DIR}/quickjs.c
    ${QJS_DIR}/libregexp.c
    ${QJS_DIR}/libunicode.c
    ${QJS_DIR}/cutils.c
    ${QJS_DIR}/dtoa.c
)
target_include_directories(quickjs PUBLIC ${QJS_DIR})
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE CONFIG_VERSION="2024-01-13" CONFIG_BIGNUM)

if(UNIX)
    target_link_libraries(quickjs PRIVATE m pthread $<$<NOT:$<BOOL:${APPLE}>>:dl>)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quickjs PRIVATE -w)
endif()

include_directories(include/builtins)
include_directories(include)

add_executable(capy
 src/capy_main.cc
 src/builtins/builtins.cc
 src/builtins/console.cc
 src/builtins/request.cc
)
 
add_library(server src/server.cc)
target_include_directories(server PRIVATE ${QJS_DIR})  # ← add this
target_link_libraries(server uv_a quickjs)
target_link_libraries(capy server uv_a quickjs)


# -- Tests ---------------------------------------------
add_executable(mem_pool_test
    tests/mem_pool_test.cc
)

add_executable(mem_pool_perf
    tests/mem_pool_perf.cc
)

add_executable(client_state_test
    tests/client_state_test.cc
)

target_link_libraries(client_state_test uv quickjs)