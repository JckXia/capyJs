cmake_minimum_required(VERSION 3.21)
project(capyJs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CAPY_STATIC "Statically link the capy executable" OFF)
option(CAPY_ASAN   "Build with AddressSanitizer"          OFF)

# All vendored/fetched deps build as static libs regardless of preset.
# CAPY_STATIC separately controls whether the final binary is fully statically linked.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Use -O2 for release instead of CMake's default -O3
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE   "-O2 -DNDEBUG" CACHE STRING "" FORCE)

if(CAPY_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address)
endif()

set(PICO_HTTP_DIR ${CMAKE_SOURCE_DIR}/deps/picohttpparser)
add_library(picohttp STATIC 
   ${PICO_HTTP_DIR}/picohttpparser.c
)
target_include_directories(picohttp PUBLIC ${PICO_HTTP_DIR})
# add_library(http_parser STATIC deps/http-parser/http_parser.c)
# target_include_directories(http_parser PUBLIC deps/http-parser)
#----------------------
# libuv (vendored)
#----------------------
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
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

#----------------------
# llama.cpp
#----------------------
include(FetchContent)
FetchContent_Declare(
    llama_cpp
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
    GIT_TAG        b5616
    GIT_SHALLOW    TRUE
)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
if(CMAKE_CROSSCOMPILING)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
else()
    set(GGML_NATIVE ON  CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(llama_cpp)

include_directories(include/builtins)
include_directories(include)

add_executable(capy
 src/capy_main.cc
 src/util.cc
 src/timer_util.cc
 src/builtins/fs.cc
 src/http_message.cc
 src/ws.cc
 src/http_context.cc
 src/builtins/builtins.cc
 src/builtins/console.cc
 src/builtins/request.cc
 src/builtins/response.cc
 src/builtins/server_js.cc
 src/builtins/timer.cc
 src/builtins/ws_js.cc
 src/builtins/fib_js.cc
 src/builtins/js_worker_js.cc
 src/workers/fib_worker.cc
 src/workers/js_worker.cc
 src/fib_engine.cc
 src/js_engine.cc
 src/signal_context.cc
 src/inference_engine.cc
 src/workers/inference_worker.cc
 src/builtins/inference_js.cc
)

add_library(server STATIC src/server.cc)
target_include_directories(server PRIVATE ${QJS_DIR})
target_link_libraries(server uv_a quickjs picohttp)
target_include_directories(capy PRIVATE ${llama_cpp_SOURCE_DIR}/include)
target_link_libraries(capy server uv_a quickjs picohttp llama)
if(CAPY_STATIC)
    target_link_options(capy PRIVATE -static)
endif()
# -- Tests ---------------------------------------------
option(CAPY_BUILD_TESTS "Build test suite" OFF)

if(CAPY_BUILD_TESTS)
    enable_testing()

    # mem_pool — existing assert-based suite
    add_executable(mem_pool_test tests/mem_pool_test.cc)
    add_test(NAME mem_pool COMMAND mem_pool_test)

    # spsc_queue — lock-free ring queue
    add_executable(spsc_queue_test tests/spsc_queue_test.cc)
    add_test(NAME spsc_queue COMMAND spsc_queue_test)

    # work_queue — semaphore-backed SPSC wrapper (needs libuv)
    add_executable(work_queue_test tests/work_queue_test.cc)
    target_link_libraries(work_queue_test PRIVATE uv_a)
    add_test(NAME work_queue COMMAND work_queue_test)

    # conn_guard — pooled liveness token
    add_executable(conn_guard_test tests/conn_guard_test.cc)
    add_test(NAME conn_guard COMMAND conn_guard_test)
endif()